/* Pure helpers for terminal audio @-codes. No I/O; see termaudio_test.c. */

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
 ****************************************************************************/

#ifndef TERMAUDIO_H_
#define TERMAUDIO_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* "sbbs_" + 32 hex + "." + up to 8 ext chars + NUL */
#define TERMAUDIO_MAX_CACHE_NAME 48

/* Silence floor, matching termgfx's TERMGFX_DB_MUTE. */
#define TERMAUDIO_DB_MUTE  (-60.0f)
#define TERMAUDIO_DB_UNITY (0.0f)

/* Refuse an absurd boost: the APC's dB parser is unclamped. */
#define TERMAUDIO_DB_MAX   (20.0f)

#ifdef __cplusplus
extern "C" {
#endif

/* 100 -> 0 dB, <= 0 -> TERMAUDIO_DB_MUTE. The faithful percent curve,
   matching termgfx_db_from_pct(). */
float termaudio_db_from_pct(int pct);

/* Parse an @-code volume argument into decibels.
   "50" -> percent via termaudio_db_from_pct(); "-6dB"/"+3db" -> that float.
   Returns false and leaves *db untouched on anything else. */
bool termaudio_parse_volume(const char* arg, float* db);

/* Resolve an @-code file argument under text_dir. Rejects absolute paths,
   "..", backslashes, empty strings, and (when allow_subdir is false) any
   directory separator. Returns false on rejection. */
bool termaudio_resolve_path(const char* text_dir, const char* arg
                            , bool allow_subdir, char* out, size_t outsz);

/* Content-addressed client cache name: "sbbs_<32 hex>.<ext>". ext may be NULL
   or empty, and is truncated to 8 characters. */
void termaudio_cache_name(const uint8_t md5[16], const char* ext
                          , char* out, size_t outsz);

/* Parse one ESC-split token of the terminal probe reply. Returns 1 if it is
   an audio state report with libsndfile present, 0 if the report says absent,
   -1 if the token is not an audio state report at all. */
int termaudio_parse_audio_state(const char* token);

/* Parse a C;L reply body into cache names. The body's first line is the
   "SyncTERM:C;L" header; each later line is "<name> TAB <md5> LF". Ignores
   malformed lines. Returns the number of names written. */
size_t termaudio_parse_file_list(const char* body
                                 , char names[][TERMAUDIO_MAX_CACHE_NAME]
                                 , size_t maxnames);

#ifdef __cplusplus
}
#endif

#endif /* TERMAUDIO_H_ */
