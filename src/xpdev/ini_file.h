/* Functions to parse ini (initialization / configuration) files */

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

#ifndef _INI_FILE_H
#define _INI_FILE_H

#if !defined(NO_SOCKET_SUPPORT)
	#include "sockwrap.h"	/* inet_addr, SOCKET */
#endif
#include "genwrap.h"
#include "str_list.h"	/* strList_t */
#ifdef WITH_CRYPTLIB
#ifndef WITHOUT_CRYPTLIB
	#include "cryptlib.h"
#endif
#endif

#define INI_MAX_VALUE_LEN	1024		/* Maximum value length, includes '\0' */
#define ROOT_SECTION		NULL

typedef struct {
	uint		bit;
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

typedef struct {
	const char *str;
	size_t len;
} ini_lv_string_t;

typedef struct fp_list_s ini_fp_list_t;

#if defined(__cplusplus)
extern "C" {
#endif

enum iniCryptAlgo {
#if (defined(WITHOUT_CRYPTLIB) || !defined(WITH_CRYPTLIB))
	INI_CRYPT_ALGO_NONE,
#else	
	INI_CRYPT_ALGO_NONE = CRYPT_ALGO_NONE,
	INI_CRYPT_ALGO_3DES = CRYPT_ALGO_3DES,
	INI_CRYPT_ALGO_IDEA = CRYPT_ALGO_IDEA,
	INI_CRYPT_ALGO_CAST = CRYPT_ALGO_CAST,
	INI_CRYPT_ALGO_RC2 = CRYPT_ALGO_RC2,
	INI_CRYPT_ALGO_RC4 = CRYPT_ALGO_RC4,
	INI_CRYPT_ALGO_AES = CRYPT_ALGO_AES,
	INI_CRYPT_ALGO_CHACHA20 = CRYPT_ALGO_CHACHA20,
#endif
};

/* Read all section names and return as an allocated string list */
/* Optionally (if prefix!=NULL), returns a subset of section names */
DLLEXPORT str_list_t 	iniReadSectionList(FILE*, const char* prefix);
DLLEXPORT str_list_t 	iniReadSectionListWithDupes(FILE*, const char* prefix);
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
DLLEXPORT char*         iniReadSString(FILE* fp, const char* section, const char* key
                            ,const char* deflt, char* value, size_t sz);
/* If the key doesn't exist, iniReadExistingString just returns NULL */
DLLEXPORT char* 		iniReadExistingString(FILE*, const char* section, const char* key
							,const char* deflt, char* value);
DLLEXPORT str_list_t 	iniReadStringList(FILE*, const char* section, const char* key
							,const char* sep, const char* deflt);
DLLEXPORT int 			iniReadInteger(FILE*, const char* section, const char* key
							,int deflt);
DLLEXPORT uint 			iniReadUInteger(FILE*, const char* section, const char* key
							,uint deflt);
DLLEXPORT short 		iniReadShortInt(FILE*, const char* section, const char* key
							,short deflt);
DLLEXPORT ushort 		iniReadUShortInt(FILE*, const char* section, const char* key
							,ushort deflt);
DLLEXPORT long 			iniReadLongInt(FILE*, const char* section, const char* key
							,long deflt);
DLLEXPORT ulong 		iniReadULongInt(FILE*, const char* section, const char* key
							,ulong deflt);
DLLEXPORT int64_t		iniReadInt64(FILE*, const char* section, const char* key
							,int64_t deflt);
DLLEXPORT uint64_t		iniReadUInt64(FILE*, const char* section, const char* key
							,uint64_t deflt);
DLLEXPORT int64_t 		iniReadBytes(FILE*, const char* section, const char* key
							,uint unit, int64_t deflt);
DLLEXPORT double 		iniReadDuration(FILE*, const char* section, const char* key
							,double deflt);
DLLEXPORT double 		iniReadFloat(FILE*, const char* section, const char* key
							,double deflt);
DLLEXPORT bool 			iniReadBool(FILE*, const char* section, const char* key
							,bool deflt);
DLLEXPORT time_t 		iniReadDateTime(FILE*, const char* section, const char* key
							,time_t deflt);
DLLEXPORT unsigned 		iniReadEnum(FILE*, const char* section, const char* key
							,str_list_t names, unsigned deflt);
DLLEXPORT unsigned* 	iniReadEnumList(FILE*, const char* section, const char* key
							,str_list_t names, unsigned* count, const char* sep, const char* deflt);
DLLEXPORT int* 			iniReadIntList(FILE*, const char* section, const char* key
							,unsigned* count, const char* sep, const char* deflt);
DLLEXPORT int 			iniReadNamedInt(FILE*, const char* section, const char* key
							,named_int_t*, int deflt);
DLLEXPORT long			iniReadNamedLongInt(FILE*, const char* section, const char* key
							,named_long_t*, long deflt);
DLLEXPORT double 		iniReadNamedFloat(FILE*, const char* section, const char* key
							,named_double_t*, double deflt);
DLLEXPORT uint 			iniReadBitField(FILE*, const char* section, const char* key
							,ini_bitdesc_t* bitdesc, uint deflt);
#define		iniReadLogLevel(f,s,k,d) iniReadEnum(f,s,k,iniLogLevelStringList(),d)

/* Free string list returned from iniRead*List functions */
DLLEXPORT str_list_t 	iniFreeStringList(str_list_t list);

/* Free named string list returned from iniReadNamedStringList */
DLLEXPORT named_string_t** iniFreeNamedStringList(named_string_t** list);


/* File I/O Functions */
DLLEXPORT char* 		iniFileName(char* dest, size_t maxlen, const char* dir, const char* fname);
DLLEXPORT FILE* 		iniOpenFile(const char* fname, bool for_modify);
DLLEXPORT str_list_t 	iniReadFile(FILE*);
DLLEXPORT str_list_t 	iniReadFiles(FILE*, bool includes);
DLLEXPORT bool 			iniWriteFile(FILE*, const str_list_t);
DLLEXPORT bool 			iniCloseFile(FILE*);

/* StringList functions */
DLLEXPORT str_list_t 	iniGetSectionList(str_list_t list, const char* prefix);
DLLEXPORT str_list_t 	iniGetSectionListWithDupes(str_list_t list, const char* prefix);
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

/* As above but specify the size of value */
DLLEXPORT char*         iniGetSString(str_list_t listr, const char* section, const char* key
                            ,const char* deflt, char* value /* may be NULL */, size_t sz);

/* If the key doesn't exist, iniGetExistingString just returns NULL */
DLLEXPORT char* 		iniGetExistingString(str_list_t, const char* section, const char* key
							,const char* deflt, char* value /* may be NULL */);
DLLEXPORT str_list_t 	iniGetStringList(str_list_t, const char* section, const char* key
							,const char* sep, const char* deflt);
DLLEXPORT str_list_t 	iniGetSparseStringList(str_list_t, const char* section, const char* key
							,const char* sep, const char* deflt, size_t min_len);
DLLEXPORT int 			iniGetInteger(str_list_t, const char* section, const char* key
							,int deflt);
DLLEXPORT int 			iniGetIntInRange(str_list_t, const char* section, const char* key
							,int min, int deflt, int max);
DLLEXPORT int 			iniGetClampedInt(str_list_t, const char* section, const char* key
							,int min, int deflt, int max);
DLLEXPORT uint 			iniGetUInteger(str_list_t, const char* section, const char* key
							,uint deflt);
DLLEXPORT short 		iniGetShortInt(str_list_t, const char* section, const char* key
							,short deflt);
DLLEXPORT ushort 		iniGetUShortInt(str_list_t, const char* section, const char* key
							,ushort deflt);
DLLEXPORT long 			iniGetLongInt(str_list_t, const char* section, const char* key
							,long deflt);
DLLEXPORT ulong 		iniGetULongInt(str_list_t, const char* section, const char* key
							,ulong deflt);
DLLEXPORT int64_t		iniGetInt64(str_list_t, const char* section, const char* key
							,int64_t deflt);
DLLEXPORT uint64_t		iniGetUInt64(str_list_t, const char* section, const char* key
							,uint64_t deflt);
DLLEXPORT int64_t 		iniGetBytes(str_list_t, const char* section, const char* key
							,uint unit, int64_t deflt);
DLLEXPORT double 		iniGetDuration(str_list_t, const char* section, const char* key
							,double deflt);
DLLEXPORT double 		iniGetFloat(str_list_t, const char* section, const char* key
							,double deflt);
DLLEXPORT bool 			iniGetBool(str_list_t, const char* section, const char* key
							,bool deflt);
DLLEXPORT time_t 		iniGetDateTime(str_list_t, const char* section, const char* key
							,time_t deflt);
DLLEXPORT unsigned 		iniGetEnum(str_list_t, const char* section, const char* key
							,str_list_t names, unsigned deflt);
DLLEXPORT unsigned* 	iniGetEnumList(str_list_t, const char* section, const char* key
							,str_list_t names, unsigned* count, const char* sep, const char* deflt);
DLLEXPORT int* 			iniGetIntList(str_list_t, const char* section, const char* key
							,unsigned* count, const char* sep, const char* deflt);
DLLEXPORT int 			iniGetNamedInt(str_list_t, const char* section, const char* key
							,named_int_t*, int deflt);
DLLEXPORT long 			iniGetNamedLongInt(str_list_t, const char* section, const char* key
							,named_long_t*, long deflt);
DLLEXPORT double 		iniGetNamedFloat(str_list_t, const char* section, const char* key
							,named_double_t*, double deflt);
DLLEXPORT uint 			iniGetBitField(str_list_t, const char* section, const char* key
							,ini_bitdesc_t* bitdesc, uint deflt);
DLLEXPORT str_list_t 	iniGetSection(str_list_t, const char *section);
#define		iniGetLogLevel(l,s,k,d) iniGetEnum(l,s,k,iniLogLevelStringList(),d)
DLLEXPORT str_list_t 	iniCutSection(str_list_t, const char *section);

#if !defined(NO_SOCKET_SUPPORT)
// These functions return/accept IP addresses in *network* byte order
DLLEXPORT uint32_t 		iniReadIpAddress(FILE*, const char* section, const char* key
							,uint32_t deflt);
DLLEXPORT uint32_t 		iniGetIpAddress(str_list_t, const char* section, const char* key
							,uint32_t deflt);
DLLEXPORT char* 		iniSetIpAddress(str_list_t*, const char* section, const char* key, uint32_t value
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
DLLEXPORT char* 		iniSetInteger(str_list_t*, const char* section, const char* key, int value
							,ini_style_t*);
DLLEXPORT char* 		iniSetUInteger(str_list_t*, const char* section, const char* key, uint value
							,ini_style_t*);
DLLEXPORT char* 		iniSetShortInt(str_list_t*, const char* section, const char* key, short value
							,ini_style_t*);
DLLEXPORT char* 		iniSetUShortInt(str_list_t*, const char* section, const char* key, ushort value
							,ini_style_t*);
DLLEXPORT char* 		iniSetLongInt(str_list_t*, const char* section, const char* key, long value
							,ini_style_t*);
DLLEXPORT char* 		iniSetULongInt(str_list_t*, const char* section, const char* key, ulong value
							,ini_style_t*);
DLLEXPORT char* 		iniSetBytes(str_list_t*, const char* section, const char* key, uint unit, int64_t value
							,ini_style_t*);
DLLEXPORT char* 		iniSetDuration(str_list_t*, const char* section, const char* key, double value
							,ini_style_t*);
DLLEXPORT char* 		iniSetHexInt(str_list_t*, const char* section, const char* key, uint value
							,ini_style_t*);
DLLEXPORT char* 		iniSetHexInt64(str_list_t*, const char* section, const char* key, uint64_t value
							,ini_style_t*);
DLLEXPORT char* 		iniSetFloat(str_list_t*, const char* section, const char* key, double value
							,ini_style_t*);
DLLEXPORT char* 		iniSetBool(str_list_t*, const char* section, const char* key, bool value
							,ini_style_t*);
DLLEXPORT char* 		iniSetDateTime(str_list_t*, const char* section, const char* key, bool include_time, time_t
							,ini_style_t*);
DLLEXPORT char* 		iniSetEnum(str_list_t*, const char* section, const char* key, str_list_t names
							,unsigned value, ini_style_t*);
DLLEXPORT char* 		iniSetEnumList(str_list_t*, const char* section, const char* key
							,const char* sep, str_list_t names, unsigned* values, unsigned count, ini_style_t*);
DLLEXPORT char* 		iniSetNamedInt(str_list_t*, const char* section, const char* key, named_int_t*
							,int value, ini_style_t*);
DLLEXPORT char* 		iniSetNamedHexInt(str_list_t*, const char* section, const char* key, named_uint_t*
							,uint value, ini_style_t*);
DLLEXPORT char* 		iniSetNamedLongInt(str_list_t*, const char* section, const char* key, named_long_t*
							,long value, ini_style_t*);
DLLEXPORT char* 		iniSetNamedFloat(str_list_t*, const char* section, const char* key, named_double_t*
							,double value, ini_style_t*);
DLLEXPORT char* 		iniSetBitField(str_list_t*, const char* section, const char* key, ini_bitdesc_t*, uint value
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
DLLEXPORT size_t 		iniAppendSectionWithNamedStrings(str_list_t*, const char* section, const named_string_t** keys
							,ini_style_t*);

DLLEXPORT bool 			iniSectionExists(str_list_t, const char* section);
DLLEXPORT bool 			iniKeyExists(str_list_t, const char* section, const char* key);
DLLEXPORT bool 			iniValueExists(str_list_t, const char* section, const char* key);
DLLEXPORT char* 		iniPopKey(str_list_t*, const char* section, const char* key, char* value);
DLLEXPORT char* 		iniPopString(str_list_t*, const char* section, const char* key, char* value);
DLLEXPORT bool 			iniRemoveKey(str_list_t*, const char* section, const char* key);
DLLEXPORT bool 			iniRemoveValue(str_list_t*, const char* section, const char* key);
DLLEXPORT bool 			iniRemoveSection(str_list_t*, const char* section);
DLLEXPORT bool 			iniRemoveSectionFast(str_list_t, const char* section);
DLLEXPORT bool 			iniRemoveSections(str_list_t*, const char* prefix);
DLLEXPORT bool 			iniRenameSection(str_list_t*, const char* section, const char* newname);
DLLEXPORT bool 			iniSortSections(str_list_t*, const char* prefix, bool sort_keys);

DLLEXPORT bool 			iniHasInclude(const str_list_t);

/* Named String List functions */
DLLEXPORT named_str_list_t** iniParseSections(const str_list_t);
DLLEXPORT str_list_t	iniGetParsedSection(named_str_list_t**, const char* section, bool cut);
DLLEXPORT str_list_t 	iniGetParsedSectionList(named_str_list_t**, const char* prefix);
DLLEXPORT void*			iniFreeParsedSections(named_str_list_t** list);

/* Fast functions */
DLLEXPORT ini_fp_list_t * iniFastParseSections(const str_list_t list, bool orderedList);
DLLEXPORT ini_lv_string_t **iniGetFastParsedSectionList(ini_fp_list_t *fp, const char* prefix, size_t *sz);
DLLEXPORT str_list_t iniGetFastParsedSection(ini_fp_list_t *fp, const char* name, bool cut);
DLLEXPORT str_list_t iniGetFastParsedSectionLV(ini_fp_list_t *fp, ini_lv_string_t* name, bool cut);
DLLEXPORT void iniFastParsedSectionListFree(ini_lv_string_t **list);
DLLEXPORT void iniFreeFastParse(ini_fp_list_t *s);
DLLEXPORT ini_lv_string_t *iniGetFastParsedSectionOrderedList(ini_fp_list_t *fp);

/* Encryption Functions (can't do includes yet) */
DLLEXPORT str_list_t iniReadEncryptedFile(FILE* fp, bool(*get_key)(void *cb_data, char *keybuf, size_t *sz), int KDFiterations, enum iniCryptAlgo *algoPtr, int *ks, char *saltBuf, size_t *saltsz, void *cbdata);
DLLEXPORT bool iniWriteEncryptedFile(FILE* fp, const str_list_t list, enum iniCryptAlgo algo, int keySize, int KDFiterations, const char *key, char *salt);
DLLEXPORT const char *iniCryptGetAlgoName(enum iniCryptAlgo a);
DLLEXPORT enum iniCryptAlgo iniCryptGetAlgoFromName(const char *n);

/*
 * Too handy to leave internal
 */
DLLEXPORT unsigned*		parseEnumList(const char* values, const char* sep, str_list_t names, unsigned* count);
DLLEXPORT int*			parseIntList(const char* values, const char* sep, unsigned* count);

// All currently supported platforms have 16-bit shorts and 32-bit ints
#define	iniReadInt16	iniReadShortInt
#define	iniReadUInt16	iniReadUShortInt
#define iniReadInt32	iniReadInteger
#define iniReadUInt32	iniReadUInteger
#define iniGetInt16		iniGetShortInt
#define iniGetUInt16	iniGetUShortInt
#define iniGetInt32		iniGetInteger
#define iniGetUInt32	iniGetUInteger
#define iniSetInt16		iniSetShortInt
#define iniSetUInt16	iniSetUShortInt
#define iniSetInt32		iniSetInteger
#define iniSetUInt32	iniSetUInteger

#if defined(__cplusplus)
}
#endif

#endif	/* Don't add anything after this line */
