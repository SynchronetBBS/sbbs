/* Terminal audio: client-side cache tracking and APC emission */

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

#include "sbbs.h"
#include "termaudio.h"
#include "md5.h"
#include "base64.h"

/* Bytes of C;L reply we are willing to hold, and the most cache entries we
   record from it. A client holding more than this simply re-uploads the
   overflow, which costs bandwidth but is never wrong. */
#define TERMAUDIO_LIST_BUFSIZE 8192
#define TERMAUDIO_LIST_MAX     64
#define TERMAUDIO_MAX_WARNED   32

/****************************************************************************/
/* Reads an APC reply (ESC _ ... ESC \) from the remote, storing the bytes	*/
/* between the introducer and the terminator. Returns false on timeout,		*/
/* overflow, or disconnect.													*/
/****************************************************************************/
bool sbbs_t::recv_apc_reply(char* buf, size_t bufsz, size_t* len, unsigned timeout_ms)
{
	enum { state_esc, state_intro, state_body, state_body_esc } state = state_esc;
	size_t rsp = 0;
	int    ch;

	if (buf == NULL || bufsz == 0 || len == NULL)
		return false;
	*len = 0;
	time_t deadline = time(NULL) + ((timeout_ms / 1000) + 2);
	while (online && !(sys_status & SS_ABORT)) {
		if (time(NULL) > deadline)
			return false;
		if ((ch = incom(timeout_ms)) == NOINP)
			return false;
		switch (state) {
			case state_esc:
				if (ch == ESC)
					state = state_intro;
				break;
			case state_intro:
				state = (ch == '_') ? state_body : state_esc;
				break;
			case state_body:
				if (ch == ESC) {
					state = state_body_esc;
					break;
				}
				if (rsp + 1 >= bufsz)
					return false;
				buf[rsp++] = (char)ch;
				break;
			case state_body_esc:
				if (ch == '\\') {
					buf[rsp] = '\0';
					*len = rsp;
					return true;
				}
				/* A stray ESC inside the body: keep it and carry on. */
				if (rsp + 2 >= bufsz)
					return false;
				buf[rsp++] = ESC;
				buf[rsp++] = (char)ch;
				state = state_body;
				break;
		}
	}
	return false;
}

/****************************************************************************/
/* Logs at most one complaint per path per session. A display file naming a	*/
/* missing sound would otherwise log on every redraw.						*/
/****************************************************************************/
void sbbs_t::audio_warn_once(const char* path, const char* reason)
{
	if (path == NULL)
		return;
	if (strListFind(audio_warned, path, /* case_sensitive: */ true) >= 0)
		return;
	/* Bounded: @-codes are reachable from message text on a sub-board whose
	   n_pmode clears P_NOATCODES, so a single post could otherwise name an
	   unlimited number of distinct missing files. */
	if (strListCount(audio_warned) >= TERMAUDIO_MAX_WARNED)
		return;
	strListPush(&audio_warned, path);
	lprintf(LOG_WARNING, "audio file %s %s", path, reason);
}

/****************************************************************************/
/* Asks the client which of our cache entries it already holds. Issued at	*/
/* most once per session: putmsg() strips C;L out of displayed text so that	*/
/* untrusted text cannot elicit a reply, and that guarantee only holds while	*/
/* the server is the sole source of the query.								*/
/****************************************************************************/
void sbbs_t::audio_cache_list(void)
{
	char   body[TERMAUDIO_LIST_BUFSIZE];
	size_t len = 0;
	char   names[TERMAUDIO_LIST_MAX][TERMAUDIO_MAX_CACHE_NAME];
	size_t count;
	size_t i;

	audio_cache_listed = true;      /* one attempt per session, pass or fail */
	if (audio_cache_names == NULL)
		audio_cache_names = strListInit();

	term->suspend_output_rate();
	term_out("\x1b_SyncTERM:C;L;sbbs_*\x1b\\");
	bool got = recv_apc_reply(body, sizeof(body), &len, 3000);
	term->restore_output_rate();
	if (!got) {
		lprintf(LOG_DEBUG, "no C;L reply; treating the client cache as empty");
		return;                     /* empty list: everything re-uploads */
	}
	count = termaudio_parse_file_list(body, names, TERMAUDIO_LIST_MAX);
	for (i = 0; i < count; i++)
		strListPush(&audio_cache_names, names[i]);
	lprintf(LOG_DEBUG, "client already holds %u cached audio file(s)"
	        , (unsigned)count);
}

/****************************************************************************/
/* Uploads path to the client's cache unless it is already there, and writes	*/
/* the content-addressed cache name. Returns false if the file cannot be		*/
/* read or exceeds maxsize.													*/
/****************************************************************************/
bool sbbs_t::audio_cache_file(const char* path, char* cachename, size_t cnsz
                              , uint32_t maxsize)
{
	BYTE        digest[MD5_DIGEST_SIZE];
	off_t       len;
	FILE*       fp;
	char*       data;
	char*       b64;
	size_t      b64size;
	const char* ext;
	char        name[TERMAUDIO_MAX_CACHE_NAME];
	bool        ok = false;

	if (path == NULL || cachename == NULL || cnsz == 0)
		return false;
	len = flength(path);
	if (len <= 0) {
		audio_warn_once(path, "is missing or empty");
		return false;
	}
	/* maxsize bounds what may be SENT now. A larger file can still be played
	   if @CACHE_AUDIO: already put it on the client, and only the cache name,
	   which needs the contents, can tell. So before reading, refuse only what
	   neither limit allows. */
	if (len > (off_t)maxsize && len > (off_t)cfg.max_cache_file_size) {
		audio_warn_once(path, "exceeds the configured size limits");
		return false;
	}
	if ((data = (char*)malloc((size_t)len)) == NULL)
		return false;
	if ((fp = fopen(path, "rb")) == NULL) {
		free(data);
		audio_warn_once(path, "cannot be opened");
		return false;
	}
	if (fread(data, 1, (size_t)len, fp) != (size_t)len) {
		fclose(fp);
		free(data);
		audio_warn_once(path, "could not be read");
		return false;
	}
	fclose(fp);

	MD5_calc(digest, data, (size_t)len);
	ext = getfext(path);
	termaudio_cache_name(digest, (ext != NULL) ? ext + 1 : NULL, name, sizeof(name));
	if (strlen(name) >= cnsz) {
		free(data);
		return false;
	}
	strcpy(cachename, name);

	if (!audio_cache_listed)
		audio_cache_list();

	if (strListFind(audio_cache_names, name, /* case_sensitive: */ true) >= 0) {
		free(data);
		return true;                /* client holds it already; no wire traffic */
	}
	if (len > (off_t)maxsize) {
		free(data);
		audio_warn_once(path, "exceeds the configured size limit; preload it with @CACHE_AUDIO:");
		return false;
	}

	b64size = (((size_t)len + 2) / 3) * 4 + 16;
	if ((b64 = (char*)malloc(b64size)) == NULL) {
		free(data);
		return false;
	}
	if (b64_encode(b64, b64size, data, (size_t)len) > 0) {
		bool saved_lbuf = term->suspend_lbuf;
		term->suspend_output_rate();
		term->suspend_lbuf = true;
		term_out("\x1b_SyncTERM:C;S;");
		term_out(name);
		term_out(";");
		term_out(b64);
		term_out("\x1b\\");
		term->suspend_lbuf = saved_lbuf;
		term->restore_output_rate();
		strListPush(&audio_cache_names, name);
		ok = true;
	}
	free(b64);
	free(data);
	return ok;
}

/****************************************************************************/
/* Loads a cached file into a patch slot and queues it on a channel.			*/
/****************************************************************************/
void sbbs_t::audio_play(const char* cachename, bool music, float db)
{
	char     buf[TERMAUDIO_MAX_CACHE_NAME + 128];
	unsigned slot;
	unsigned chan;

	if (cachename == NULL || *cachename == '\0')
		return;
	slot = audio_next_slot;
	audio_next_slot = (audio_next_slot + 1) % (TERMAUDIO_SLOT_LAST + 1);
	if (music)
		chan = TERMAUDIO_CHAN_MUSIC;
	else {
		if (audio_next_chan < TERMAUDIO_CHAN_FIRST
		    || audio_next_chan > TERMAUDIO_CHAN_LAST)
			audio_next_chan = TERMAUDIO_CHAN_FIRST;
		chan = audio_next_chan++;
	}
	safe_snprintf(buf, sizeof(buf), "\x1b_SyncTERM:A;Load;S=%u;%s\x1b\\"
	              , slot, cachename);
	term_out(buf);
	safe_snprintf(buf, sizeof(buf), "\x1b_SyncTERM:A;Queue;C=%u;S=%u;V=%.1fdB%s\x1b\\"
	              , chan, slot, db, music ? ";L" : "");
	term_out(buf);
}

void sbbs_t::audio_flush_all(void)
{
	char     buf[64];
	unsigned chan;

	for (chan = TERMAUDIO_CHAN_MUSIC; chan <= TERMAUDIO_CHAN_LAST; chan++) {
		safe_snprintf(buf, sizeof(buf), "\x1b_SyncTERM:A;Flush;C=%u\x1b\\", chan);
		term_out(buf);
	}
}

void sbbs_t::audio_flush_music(unsigned fade_ms)
{
	char buf[64];

	if (fade_ms)
		safe_snprintf(buf, sizeof(buf), "\x1b_SyncTERM:A;Flush;C=%u;O=%u\x1b\\"
		              , TERMAUDIO_CHAN_MUSIC, fade_ms);
	else
		safe_snprintf(buf, sizeof(buf), "\x1b_SyncTERM:A;Flush;C=%u\x1b\\"
		              , TERMAUDIO_CHAN_MUSIC);
	term_out(buf);
}

void sbbs_t::audio_music_volume(float db)
{
	char buf[64];

	safe_snprintf(buf, sizeof(buf), "\x1b_SyncTERM:A;Volume;C=%u;V=%.1fdB\x1b\\"
	              , TERMAUDIO_CHAN_MUSIC, db);
	term_out(buf);
}
