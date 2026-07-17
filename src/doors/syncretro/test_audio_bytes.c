/* test_audio_bytes.c -- pin what syncretro_audio.c emits on the wire.
 *
 * The module's entire door dependency is three output calls and five config
 * getters (see syncretro.h); this stubs them and drives a scripted session
 * through the REAL termgfx audio encoder (audio.c/audio_stream.c), so the
 * call path exercised here is the one Task 3 moves into libtermgfx. It is
 * the guard for that move: run this before and after the extraction and the
 * transcript must not change.
 *
 * TRANSCRIPT FORM. An Ogg stream carries a random bitstream serial number --
 * confirmed empirically (encode the same PCM twice, `cmp` the output: differs
 * at the OggS page header's serial field every time) -- so a raw-byte golden
 * would be a permanent false alarm. This harness instead normalizes: one line
 * per emitted APC (SyncTERM's "ESC _ SyncTERM:<verb>;<params>ESC \" wire
 * form), giving the verb, its parameters, and a payload's LENGTH but never
 * its content. That is also the right unit for the extraction this guards --
 * it can only change call sequencing, not encoding, and the encoder is the
 * same code on both sides of the move.
 *
 * Every emitted APC arrives as exactly one sr_out_put() call: sr_apc() in
 * syncretro_audio.c passes one builder's whole return length through
 * verbatim, never splitting or coalescing. So ORDER of sr_out_put() calls IS
 * order of APCs, and that is what ties chunks-in vs. bytes-out together in
 * the golden below (cache-name rotation, the PRIME hold/release batch, the
 * cached-silence no-Cache path, the held-not-queued shape of a re-prime, and
 * the sustained-backlog drop all show up as a difference in the SEQUENCE of
 * lines, not any one line's content).
 *
 * File-static state in syncretro_audio.c has no reset (see its header
 * comment), so this binary runs exactly ONE scripted scenario per process.
 *
 * Usage:
 *   test_audio_bytes                 -- print the transcript to stdout
 *   test_audio_bytes --check FILE    -- compare against a golden, exit
 *                                        nonzero on any mismatch
 */
#include "syncretro_audio.h"
#include "syncretro.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- door-seam stubs: config getters -------------------------------------
 * Values mirror syncretro_config.c's compiled-in defaults (syncretro.h's
 * comments), so the scenario matches what a real session reads. */

static int    g_cfg_enabled   = 1;
static double g_cfg_quality   = 0.15;   /* TERMGFX_MUSIC_QUALITY_DEFAULT */
static int    g_cfg_volume    = 100;
static int    g_cfg_chunk_ms  = 100;
static int    g_cfg_prebuffer = 3;

int sr_config_audio_enabled(void)
{
	return g_cfg_enabled;
}

double sr_config_audio_quality(void)
{
	return g_cfg_quality;
}

int sr_config_audio_volume(void)
{
	return g_cfg_volume;
}

int sr_config_audio_chunk_ms(void)
{
	return g_cfg_chunk_ms;
}

int sr_config_audio_prebuffer(void)
{
	return g_cfg_prebuffer;
}

/* --- door-seam stubs: I/O -------------------------------------------------
 * g_backlog is scenario-controlled (step 6 drives it over the drop
 * threshold); g_flush_calls counts sr_io_out_flush() invocations, part of
 * the contract the golden pins (a PRIME release and a shutdown each flush
 * exactly once). */

static size_t g_backlog;
static int    g_flush_calls;

int sr_io_out_flush(void)
{
	g_flush_calls++;
	return 0;
}

size_t sr_io_out_backlog(void)
{
	return g_backlog;
}

/* Every APC lands here as one whole, self-delimited record (see the file
 * header comment) -- capture it verbatim, in order. */

#define MAX_RECORDS 512
static struct {
	uint8_t *data;
	size_t   len;
} g_rec[MAX_RECORDS];
static int g_rec_n;

void sr_out_put(const void *buf, size_t len)
{
	uint8_t *copy;

	if (g_rec_n >= MAX_RECORDS) {
		fprintf(stderr, "test_audio_bytes: MAX_RECORDS exceeded\n");
		exit(2);
	}
	copy = malloc(len ? len : 1);
	if (copy == NULL) {
		fprintf(stderr, "test_audio_bytes: OOM\n");
		exit(2);
	}
	memcpy(copy, buf, len);
	g_rec[g_rec_n].data = copy;
	g_rec[g_rec_n].len  = len;
	g_rec_n++;
}

/* --- normalized transcript ------------------------------------------------
 * Every builder in ../termgfx/audio.c writes "ESC _ SyncTERM:<verb>;<params>
 * ESC \" (audio.h's file header); no ';' occurs inside a base64 payload
 * (RFC 4648 alphabet), so splitting the body on ';' is exact. */

static size_t b64_decoded_len(const char *b64, size_t b64len)
{
	size_t pad = 0;

	if (b64len >= 1 && b64[b64len - 1] == '=')
		pad++;
	if (b64len >= 2 && b64[b64len - 2] == '=')
		pad++;
	return (b64len / 4) * 3 - pad;
}

/* "S=0" -> "0"; a token with no '=' is returned as-is. */
static const char *kv(const char *tok)
{
	const char *eq = strchr(tok, '=');

	return eq ? eq + 1 : tok;
}

static void print_record(FILE *out, int idx, const uint8_t *data, size_t len)
{
	static const char prefix[] = "\x1b_SyncTERM:";
	static const char suffix[] = "\x1b\\";
	size_t             plen    = sizeof prefix - 1;
	size_t             slen    = sizeof suffix - 1;
	char *             body;
	char *             body_orig;
	size_t             blen;
	char *             toks[8];
	int                ntok;
	char *             tok;

	if (len < plen + slen ||
	    memcmp(data, prefix, plen) != 0 ||
	    memcmp(data + len - slen, suffix, slen) != 0) {
		fprintf(out, "%03d ??? unrecognized record, len=%lu\n",
		        idx, (unsigned long)len);
		return;
	}

	blen      = len - plen - slen;
	body      = malloc(blen + 1);
	body_orig = malloc(blen + 1);
	memcpy(body, data + plen, blen);
	memcpy(body_orig, data + plen, blen);
	body[blen]      = '\0';
	body_orig[blen] = '\0';

	ntok = 0;
	tok  = strtok(body, ";");
	while (tok != NULL && ntok < 8) {
		toks[ntok++] = tok;
		tok          = strtok(NULL, ";");
	}

	if (ntok >= 2 && strcmp(toks[0], "Q") == 0) {
		fprintf(out, "%03d Q;%s\n", idx, toks[1]);
	} else if (ntok == 4 && strcmp(toks[0], "C") == 0 && strcmp(toks[1], "S") == 0) {
		fprintf(out, "%03d C;S name=%s bytes=%lu\n", idx, toks[2],
		        (unsigned long)b64_decoded_len(toks[3], strlen(toks[3])));
	} else if (ntok == 4 && strcmp(toks[0], "A") == 0 && strcmp(toks[1], "Load") == 0) {
		fprintf(out, "%03d A;Load slot=%s file=%s\n", idx, kv(toks[2]), toks[3]);
	} else if (ntok == 4 && strcmp(toks[0], "A") == 0 && strcmp(toks[1], "LoadBlob") == 0) {
		fprintf(out, "%03d A;LoadBlob slot=%s bytes=%lu\n", idx, kv(toks[2]),
		        (unsigned long)b64_decoded_len(toks[3], strlen(toks[3])));
	} else if ((ntok == 6 || ntok == 7) &&
	           strcmp(toks[0], "A") == 0 && strcmp(toks[1], "Queue") == 0) {
		fprintf(out, "%03d A;Queue ch=%s slot=%s vol=%s loop=%d\n", idx,
		        kv(toks[2]), kv(toks[3]), kv(toks[4]), ntok == 7 ? 1 : 0);
	} else if (ntok == 4 && strcmp(toks[0], "A") == 0 && strcmp(toks[1], "Volume") == 0) {
		fprintf(out, "%03d A;Volume ch=%s vol=%s\n", idx, kv(toks[2]), kv(toks[3]));
	} else if ((ntok == 3 || ntok == 4) &&
	           strcmp(toks[0], "A") == 0 && strcmp(toks[1], "Flush") == 0) {
		fprintf(out, "%03d A;Flush ch=%s fade=%s\n", idx, kv(toks[2]),
		        ntok == 4 ? kv(toks[3]) : "0");
	} else if (ntok == 3 && strcmp(toks[0], "A") == 0 && strcmp(toks[1], "Update") == 0) {
		fprintf(out, "%03d A;Update ch=%s\n", idx, kv(toks[2]));
	} else {
		fprintf(out, "%03d ??? %s\n", idx, body_orig);
	}

	free(body);
	free(body_orig);
}

/* --- scripted PCM feed -----------------------------------------------------
 * `phase` is the running frame index across the whole session (never reset
 * per call), so the ramp value is a deterministic function of overall frame
 * index -- never silent, never random, never a repeating short cycle that
 * could accidentally alias into a fixed pattern. */

static void feed_ramp(size_t frames, uint32_t *phase)
{
	int16_t *pcm = malloc(frames * 2 * sizeof(int16_t));
	size_t   i;

	for (i = 0; i < frames; i++) {
		int16_t v = (int16_t)((int32_t)((*phase + i) % 40000) - 20000);

		pcm[i * 2]     = v;
		pcm[i * 2 + 1] = v;
	}
	sr_audio_feed(pcm, frames);
	*phase += (uint32_t)frames;
	free(pcm);
}

static void feed_zeros(size_t frames)
{
	int16_t *pcm = calloc(frames * 2, sizeof(int16_t));

	sr_audio_feed(pcm, frames);
	free(pcm);
}

/* --- the scripted session (brief step numbers in the comments) -----------
 * 100ms chunks @ 44100 Hz = 4410 frames/chunk (sr_audio_start's sizing). */

#define FRAMES_PER_CHUNK 4410

static void run_scenario(void)
{
	uint32_t phase = 0;

	/* 1: sr_audio_init/start/probe. */
	sr_audio_init();
	sr_audio_start(44100);
	sr_audio_probe();

	/* 2: digital tier -- channel volume, then the silence upload. */
	sr_audio_caps(1);

	/* 3: a ramp long enough to close 5 chunks -- prebuffer=3 holds chunks
	 * 1-3 and releases them together on the 3rd close (PRIME -> RUN);
	 * chunks 4-5 close in steady-state RUN and send immediately. */
	feed_ramp(FRAMES_PER_CHUNK * 5, &phase);

	/* 4: one full chunk of pure zeros -- the cached-silence path (s/z was
	 * uploaded once in step 2): Load+Queue+Update, no C;S. */
	feed_zeros(FRAMES_PER_CHUNK);

	/* 5: an injected underrun re-primes rather than resuming RUN. The very
	 * next chunk closed is HELD (no APC at all for it) instead of being
	 * queued immediately, and it takes `prebuffer` (3) more closes before
	 * the held batch is released as a group -- that shape (a gap, then a
	 * 3-chunk burst) is what this step pins. */
	sr_audio_underrun(SR_AUDIO_CH);
	feed_ramp(FRAMES_PER_CHUNK * 3, &phase);

	/* 6: sustained backlog over two consecutive chunk boundaries drops the
	 * second chunk outright (no APC for it); the first, still under the
	 * 2-strike threshold, sends normally. */
	g_backlog = 48 * 1024 + 1;    /* > TERMGFX_STREAM_BACKLOG_BYTES */
	feed_ramp(FRAMES_PER_CHUNK, &phase);    /* strike 1: still sends */
	feed_ramp(FRAMES_PER_CHUNK, &phase);    /* strike 2: dropped */
	g_backlog = 0;

	/* 7: shutdown -- A;Flush on the channel, no fade. */
	sr_audio_shutdown();
}

int main(int argc, char **argv)
{
	int   check_mode = (argc >= 3 && strcmp(argv[1], "--check") == 0);
	FILE *out        = check_mode ? tmpfile() : stdout;
	int   i;

	if (out == NULL) {
		fprintf(stderr, "test_audio_bytes: tmpfile() failed\n");
		return 2;
	}

	run_scenario();

	for (i = 0; i < g_rec_n; i++)
		print_record(out, i, g_rec[i].data, g_rec[i].len);
	fprintf(out, "-- %d record(s), %d sr_io_out_flush() call(s) --\n",
	        g_rec_n, g_flush_calls);

	if (!check_mode)
		return 0;

	{
		FILE * golden;
		char * got;
		char * want;
		long   got_len;
		long   want_len;
		int    mismatch;

		golden = fopen(argv[2], "rb");
		if (golden == NULL) {
			fprintf(stderr, "test_audio_bytes: cannot open golden %s\n", argv[2]);
			return 2;
		}

		fseek(out, 0, SEEK_END);
		got_len = ftell(out);
		fseek(out, 0, SEEK_SET);
		got = malloc((size_t)got_len + 1);
		fread(got, 1, (size_t)got_len, out);
		got[got_len] = '\0';

		fseek(golden, 0, SEEK_END);
		want_len = ftell(golden);
		fseek(golden, 0, SEEK_SET);
		want = malloc((size_t)want_len + 1);
		fread(want, 1, (size_t)want_len, golden);
		want[want_len] = '\0';
		fclose(golden);

		mismatch = (got_len != want_len) || memcmp(got, want, (size_t)got_len) != 0;
		if (mismatch) {
			fprintf(stderr, "FAIL: transcript does not match %s\n", argv[2]);
			fprintf(stderr, "--- got (%ld bytes) ---\n%s\n", got_len, got);
			fprintf(stderr, "--- want (%ld bytes) ---\n%s\n", want_len, want);
		} else {
			printf("ok: transcript matches %s (%ld bytes, %d records)\n",
			       argv[2], got_len, g_rec_n);
		}
		free(got);
		free(want);
		return mismatch ? 1 : 0;
	}
}
