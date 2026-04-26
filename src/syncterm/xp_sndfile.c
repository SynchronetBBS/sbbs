/*
 * xp_sndfile.c — syncterm-local libsndfile wrapper (see xp_sndfile.h).
 *
 * All three build modes (WITHOUT / WITH / STATIC SNDFILE) produce a
 * pair of callable symbols (sndfile_available + sndfile_decode) and
 * share the same decode-and-resample logic; only the symbol-lookup
 * strategy changes.  See the macro / typedef switch below for the
 * WITH_SNDFILE dlopen indirection.
 */

#include "xp_sndfile.h"

#include <stdlib.h>   /* malloc, free */
#include <string.h>   /* memset */

#if defined(WITHOUT_SNDFILE)

/* Fully stubbed build — no libsndfile references at all. */
bool sndfile_available(void)
{
	return false;
}

bool sndfile_decode(const char *path, int16_t **out_frames, size_t *out_nframes)
{
	(void)path;
	if (out_frames)  *out_frames  = NULL;
	if (out_nframes) *out_nframes = 0;
	return false;
}

#else /* !WITHOUT_SNDFILE */

#include <sndfile.h>

#if defined(STATIC_SNDFILE)

/* Direct-linkage build.  sndfile_available() is always true; the
 * symbol-lookup layer is a no-op. */
static bool
sndfile_ensure_loaded(void)
{
	return true;
}

#define SF_CALL_open(path, mode, info)          sf_open(path, mode, info)
#define SF_CALL_readf_short(sf, ptr, frames)    sf_readf_short(sf, ptr, frames)
#define SF_CALL_close(sf)                       sf_close(sf)

bool
sndfile_available(void)
{
	return true;
}

#else /* runtime WITH_SNDFILE path */

#include <xp_dl.h>   /* xp_dlopen / xp_dlsym */

/* Field names avoid bare `open` / `close` — some platforms' system
 * headers (Alpine musl under _FORTIFY_SOURCE) turn those into macros,
 * which would then rewrite `sf_api.close` into nonsense. */
static struct sndfile_api {
	dll_handle   dl;
	SNDFILE *  (*open_fn)(const char *path, int mode, SF_INFO *info);
	sf_count_t (*readf_short)(SNDFILE *sf, short *ptr, sf_count_t frames);
	int        (*close_fn)(SNDFILE *sf);
} sf_api;

static bool sndfile_load_attempted = false;
static bool sndfile_load_success   = false;

static bool
sndfile_ensure_loaded(void)
{
	/* Library names xp_dlopen will try in order — xp_dl.c handles the
	 * platform suffixes (.so.1, .1.dylib, .dll). */
	const char *names[] = { "sndfile", "libsndfile", NULL };

	if (sndfile_load_attempted)
		return sndfile_load_success;
	sndfile_load_attempted = true;

	sf_api.dl = xp_dlopen(names, RTLD_LAZY, 1);
	if (sf_api.dl == NULL)
		return false;

	sf_api.open_fn     = xp_dlsym(sf_api.dl, sf_open);
	sf_api.readf_short = xp_dlsym(sf_api.dl, sf_readf_short);
	sf_api.close_fn    = xp_dlsym(sf_api.dl, sf_close);

	if (sf_api.open_fn == NULL || sf_api.readf_short == NULL || sf_api.close_fn == NULL) {
		xp_dlclose(sf_api.dl);
		sf_api.dl = NULL;
		return false;
	}
	sndfile_load_success = true;
	return true;
}

#define SF_CALL_open(path, mode, info)          sf_api.open_fn(path, mode, info)
#define SF_CALL_readf_short(sf, ptr, frames)    sf_api.readf_short(sf, ptr, frames)
#define SF_CALL_close(sf)                       sf_api.close_fn(sf)

bool
sndfile_available(void)
{
	return sndfile_ensure_loaded();
}

#endif /* STATIC_SNDFILE vs dynamic */

/* ----------------------------------------------------------------- */
/* Decode + resample + channel remap — shared across STATIC_SNDFILE  */
/* and runtime WITH_SNDFILE builds.                                  */
/* ----------------------------------------------------------------- */

#define XPBEEP_TARGET_RATE  44100  /* matches xpbeep.h — avoid pulling that
                                    * header in just for the constant. */

/* Linear-interpolation upsample/downsample from `src_rate` to 44100,
 * and channel-remap to stereo.  Input is interleaved int16_t with
 * `src_channels` channels.  Output is freshly allocated interleaved
 * int16_t stereo (L, R, L, R, ...).
 *
 * Channel remap rules:
 *   1 ch (mono)      → L = R = mono
 *   2 ch (stereo)    → pass through
 *   N > 2 ch         → take the first two channels (L, R)
 *
 * Returns NULL on allocation failure. */
static int16_t *
resample_to_s16_stereo_44100(const int16_t *src, size_t src_nframes, int src_channels,
                             int src_rate, size_t *out_nframes)
{
	int16_t *dst;
	size_t   dst_nframes;
	size_t   j;

	if (src_rate <= 0 || src_channels <= 0 || src_nframes == 0) {
		*out_nframes = 0;
		return NULL;
	}

	/* dst = src * 44100 / src_rate.  Round to nearest; keep at least 1. */
	dst_nframes = (size_t)(((uint64_t)src_nframes * XPBEEP_TARGET_RATE + (src_rate / 2)) / src_rate);
	if (dst_nframes == 0)
		dst_nframes = 1;

	dst = (int16_t *)malloc(dst_nframes * 2 * sizeof(int16_t));
	if (dst == NULL) {
		*out_nframes = 0;
		return NULL;
	}

	for (j = 0; j < dst_nframes; j++) {
		/* Fractional source index in 32.32 fixed-point.  Compute the
		 * integer frame index and the remainder separately so we
		 * never form `j * src_rate * 2^32` — that overflows uint64
		 * at about 2 seconds of 44.1 kHz source and produces wrapped
		 * indices that look like "the file looped back to the start
		 * mid-decode".  whole_pos and rem*2^32 each stay within
		 * uint64 range for any practical file length. */
		uint64_t whole_pos = (uint64_t)j * (uint64_t)src_rate;
		uint64_t idx_int   = whole_pos / (uint64_t)XPBEEP_TARGET_RATE;
		uint64_t rem       = whole_pos - idx_int * (uint64_t)XPBEEP_TARGET_RATE;
		uint32_t idx_frac  = (uint32_t)((rem * 0x100000000ULL) /
		                                 (uint64_t)XPBEEP_TARGET_RATE);
		int      lidx_a;
		int      lidx_b;
		int      ridx_a;
		int      ridx_b;
		int32_t  la;
		int32_t  lb;
		int32_t  ra;
		int32_t  rb;
		int32_t  l_interp;
		int32_t  r_interp;

		/* Clamp at the last source frame. */
		if (idx_int >= src_nframes) {
			idx_int  = src_nframes - 1;
			idx_frac = 0;
		}

		lidx_a = (int)idx_int * src_channels;
		lidx_b = (idx_int + 1 < src_nframes) ? lidx_a + src_channels : lidx_a;
		ridx_a = lidx_a;
		ridx_b = lidx_b;
		if (src_channels >= 2) {
			ridx_a = lidx_a + 1;
			ridx_b = lidx_b + 1;
		}

		la = src[lidx_a];
		lb = src[lidx_b];
		ra = src[ridx_a];
		rb = src[ridx_b];
		/* Linear interp: a + (b - a) * frac / 2^32.  Do the
		 * multiplication in int64 to keep the full precision. */
		l_interp = (int32_t)(la + (((int64_t)(lb - la) * idx_frac) >> 32));
		r_interp = (int32_t)(ra + (((int64_t)(rb - ra) * idx_frac) >> 32));

		if (l_interp >  32767) l_interp =  32767;
		if (l_interp < -32768) l_interp = -32768;
		if (r_interp >  32767) r_interp =  32767;
		if (r_interp < -32768) r_interp = -32768;
		dst[j * 2 + 0] = (int16_t)l_interp;
		dst[j * 2 + 1] = (int16_t)r_interp;
	}

	*out_nframes = dst_nframes;
	return dst;
}

bool
sndfile_decode(const char *path, int16_t **out_frames, size_t *out_nframes)
{
	SNDFILE     *sf        = NULL;
	SF_INFO      info;
	int16_t     *raw       = NULL;
	int16_t     *converted = NULL;
	size_t       raw_bytes;
	sf_count_t   read;
	size_t       converted_n;

	if (out_frames)  *out_frames  = NULL;
	if (out_nframes) *out_nframes = 0;
	if (path == NULL || out_frames == NULL || out_nframes == NULL)
		return false;
	if (!sndfile_ensure_loaded())
		return false;

	memset(&info, 0, sizeof(info));
	sf = SF_CALL_open(path, SFM_READ, &info);
	if (sf == NULL)
		return false;
	if (info.frames <= 0 || info.channels <= 0 || info.samplerate <= 0)
		goto fail;

	/* Allocate a buffer for the native interleaved int16 read.  One
	 * frame = `info.channels` samples; libsndfile handles the internal
	 * format conversion to S16 via sf_readf_short. */
	raw_bytes = (size_t)info.frames * (size_t)info.channels * sizeof(int16_t);
	raw = (int16_t *)malloc(raw_bytes);
	if (raw == NULL)
		goto fail;
	read = SF_CALL_readf_short(sf, raw, info.frames);
	if (read <= 0)
		goto fail;

	converted = resample_to_s16_stereo_44100(raw, (size_t)read,
	                                         info.channels, info.samplerate,
	                                         &converted_n);
	if (converted == NULL)
		goto fail;

	SF_CALL_close(sf);
	free(raw);
	*out_frames  = converted;
	*out_nframes = converted_n;
	return true;

fail:
	if (sf != NULL)
		SF_CALL_close(sf);
	free(raw);
	return false;
}

#endif /* !WITHOUT_SNDFILE */
