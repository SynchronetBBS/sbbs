// audio.c -- SyncTERM "SyncTERM:A" audio-APC transport (Store/Load/Queue/Synth/
// Flush) + the libsndfile capability probe. See audio.h. I/O-free, in the style
// of apc.c / caps.c: the door sends the bytes and owns the read loop.

#include "audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef TERMGFX_WITH_SNDFILE
#include <sndfile.h>
#endif

// base64 (RFC 4648, no line breaks) -> SyncTERM's b64_decode_alloc(). Mirrors
// apc.c's encoder; kept local so the module stays self-contained.
static const char b64tab[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static size_t b64_encode(char *out, const uint8_t *in, size_t len)
{
	size_t i, o = 0;

	for (i = 0; i + 2 < len; i += 3) {
		uint32_t v = (in[i] << 16) | (in[i + 1] << 8) | in[i + 2];
		out[o++] = b64tab[(v >> 18) & 0x3f];
		out[o++] = b64tab[(v >> 12) & 0x3f];
		out[o++] = b64tab[(v >>  6) & 0x3f];
		out[o++] = b64tab[ v        & 0x3f];
	}
	if (i < len) {
		uint32_t v = in[i] << 16;
		if (i + 1 < len)
			v |= in[i + 1] << 8;
		out[o++] = b64tab[(v >> 18) & 0x3f];
		out[o++] = b64tab[(v >> 12) & 0x3f];
		out[o++] = (i + 1 < len) ? b64tab[(v >> 6) & 0x3f] : '=';
		out[o++] = '=';
	}
	return o;
}

static void le16(uint8_t *p, uint16_t v)
{
	p[0] = v & 0xff;
	p[1] = (v >> 8) & 0xff;
}

static void le32(uint8_t *p, uint32_t v)
{
	p[0] = v & 0xff;
	p[1] = (v >> 8) & 0xff;
	p[2] = (v >> 16) & 0xff;
	p[3] = (v >> 24) & 0xff;
}

// Canonical 44-byte PCM WAV header into h[0..43] for `datalen` bytes of audio.
static void wav_header(uint8_t *h, size_t datalen, int bits, int ch, int rate)
{
	uint32_t byterate   = (uint32_t)rate * ch * bits / 8;
	uint16_t blockalign = (uint16_t)(ch * bits / 8);

	memcpy(h + 0, "RIFF", 4);
	le32(h + 4, (uint32_t)(36 + datalen));
	memcpy(h + 8, "WAVE", 4);
	memcpy(h + 12, "fmt ", 4);
	le32(h + 16, 16);                       // fmt chunk size
	le16(h + 20, 1);                        // PCM
	le16(h + 22, (uint16_t)ch);
	le32(h + 24, (uint32_t)rate);
	le32(h + 28, byterate);
	le16(h + 32, blockalign);
	le16(h + 34, (uint16_t)bits);
	memcpy(h + 36, "data", 4);
	le32(h + 40, (uint32_t)datalen);
}

// ---- capability probe ----------------------------------------------------

// APC: ESC _ SyncTERM:Q;libsndfile ST  (ST = ESC backslash)
const char *const termgfx_audio_query = "\x1b_SyncTERM:Q;libsndfile\x1b\\";

int termgfx_audio_parse_caps(const uint8_t *acc, int len)
{
	int j;

	// reply: ESC [ = 7 ; 1 0 0 ; {0,1} n
	for (j = 0; j + 9 < len; j++) {
		if (acc[j] == 0x1b && acc[j + 1] == '[' && acc[j + 2] == '=' &&
		    acc[j + 3] == '7' && acc[j + 4] == ';' &&
		    acc[j + 5] == '1' && acc[j + 6] == '0' && acc[j + 7] == '0' &&
		    acc[j + 8] == ';' &&
		    (acc[j + 9] == '0' || acc[j + 9] == '1'))
			return acc[j + 9] - '0';
	}
	return -1;
}

// ---- builders ------------------------------------------------------------

size_t termgfx_audio_cache_file(uint8_t **buf, size_t *cap, const char *file,
                                const void *data, size_t bytes)
{
	size_t   b64len = 4 * ((bytes + 2) / 3);
	size_t   need   = strlen(file) + b64len + 64;
	uint8_t *p;

	if (need > *cap) {
		*buf = realloc(*buf, need);
		*cap = need;
	}
	p  = *buf;
	p += sprintf((char *)p, "\x1b_SyncTERM:C;S;%s;", file);     // Store header
	p += b64_encode((char *)p, data, bytes);                   // base64 payload
	memcpy(p, "\x1b\\", 2); p += 2;                            // ST
	return (size_t)(p - *buf);
}

size_t termgfx_audio_cache_pcm(uint8_t **buf, size_t *cap, const char *file,
                               const void *data, size_t bytes,
                               int bits, int channels, int rate)
{
	size_t   wavlen = 44 + bytes;
	size_t   n;
	uint8_t *wav;

	wav = malloc(wavlen);
	if (wav == NULL)
		return 0;
	wav_header(wav, bytes, bits, channels, rate);
	memcpy(wav + 44, data, bytes);
	n = termgfx_audio_cache_file(buf, cap, file, wav, wavlen);  // wrap-then-Store
	free(wav);
	return n;
}

// ---- Ogg/Opus encode (optional, libsndfile) ------------------------------

#ifdef TERMGFX_WITH_SNDFILE

// libsndfile virtual-I/O sink: it writes the encoded Ogg into a growing malloc
// buffer instead of a file, so we can base64 it straight into a C;S Store.
struct memsink {
	uint8_t *data;
	sf_count_t len, cap, pos;
};

static sf_count_t ms_filelen(void *u) { return ((struct memsink *)u)->len; }
static sf_count_t ms_tell(void *u)    { return ((struct memsink *)u)->pos; }

static sf_count_t ms_seek(sf_count_t off, int whence, void *u)
{
	struct memsink *s = u;
	sf_count_t      np = (whence == SEEK_SET) ? off
	                   : (whence == SEEK_CUR) ? s->pos + off
	                   : s->len + off;

	if (np < 0)
		np = 0;
	s->pos = np;
	return s->pos;
}

static sf_count_t ms_read(void *ptr, sf_count_t n, void *u)
{
	struct memsink *s     = u;
	sf_count_t      avail = s->len - s->pos;

	if (n > avail)
		n = avail;
	if (n > 0) {
		memcpy(ptr, s->data + s->pos, (size_t)n);
		s->pos += n;
	}
	return n;
}

static sf_count_t ms_write(const void *ptr, sf_count_t n, void *u)
{
	struct memsink *s = u;

	if (s->pos + n > s->cap) {
		sf_count_t ncap = s->cap ? s->cap * 2 : 65536;
		uint8_t *  nd;

		while (ncap < s->pos + n)
			ncap *= 2;
		nd = realloc(s->data, (size_t)ncap);
		if (nd == NULL)
			return 0;
		s->data = nd;
		s->cap  = ncap;
	}
	memcpy(s->data + s->pos, ptr, (size_t)n);
	s->pos += n;
	if (s->pos > s->len)
		s->len = s->pos;
	return n;
}

int termgfx_audio_have_ogg(void) { return 1; }

// libsndfile's Opus encoder accepts only these sample rates. Kept sorted
// ascending -- termgfx_audio_pick_opus_rate() relies on that.
static const int opus_rates[] = { 8000, 12000, 16000, 24000, 48000 };
#define OPUS_RATE_COUNT (int)(sizeof(opus_rates) / sizeof(opus_rates[0]))

static int termgfx_audio_is_opus_rate(int rate)
{
	int i;

	for (i = 0; i < OPUS_RATE_COUNT; i++) {
		if (opus_rates[i] == rate)
			return 1;
	}
	return 0;
}

// Smallest Opus-legal rate that is >= `rate`; anything above 48000 (Opus's
// largest legal rate -- not reachable by any current caller) falls back to
// 48000, i.e. DOWN-samples. For every rate this function actually sees
// today the result is >= the input, so the plain linear interpolation in
// termgfx_audio_resample_linear() below only ever up-samples and can't
// introduce aliasing (a true down-sample would need a low-pass filter
// first) -- e.g. Red Alert's native 22050 Hz AUD tracks
// (src/doors/syncconquer) land on 24000 Hz, the closest legal rate above
// it.
static int termgfx_audio_pick_opus_rate(int rate)
{
	int i;

	for (i = 0; i < OPUS_RATE_COUNT; i++) {
		if (opus_rates[i] >= rate)
			return opus_rates[i];
	}
	return 48000;
}

// Linear-interpolation resample of interleaved 16-bit PCM from `in_rate` to
// `out_rate` (up-sampling only -- see termgfx_audio_pick_opus_rate()).
// Returns a malloc'd buffer of `*out_frames` frames in *out (caller frees),
// or 0 on failure. This is a "good enough for game music" quality choice,
// not archival-grade resampling: a proper windowed-sinc filter would sound
// better but is unnecessary work for what amounts to a ~1.1x-1.2x rate
// bump on already-lossy-compressed source material, and linear
// interpolation is still a real step up from (and never worse than) the
// nearest-neighbor/zero-order-hold shortcut that would otherwise be
// tempting here.
static int16_t *termgfx_audio_resample_linear(const int16_t *in, size_t in_frames,
                                              int channels, int in_rate, int out_rate,
                                              size_t *out_frames)
{
	double   ratio = (double)in_rate / (double)out_rate;
	size_t   frames;
	int16_t *dst;
	size_t   i;

	*out_frames = 0;
	if (in == NULL || in_frames == 0 || channels <= 0 || in_rate <= 0 || out_rate <= 0)
		return NULL;
	frames = (size_t)((double)in_frames * (double)out_rate / (double)in_rate + 0.5);
	if (frames == 0)
		return NULL;
	dst = malloc(frames * (size_t)channels * sizeof(int16_t));
	if (dst == NULL)
		return NULL;
	for (i = 0; i < frames; i++) {
		double srcpos = (double)i * ratio;
		size_t i0     = (size_t)srcpos;
		double frac   = srcpos - (double)i0;
		size_t i1     = (i0 + 1 < in_frames) ? i0 + 1 : in_frames - 1;
		int    c;

		if (i0 >= in_frames)
			i0 = in_frames - 1;
		for (c = 0; c < channels; c++) {
			double s0 = in[i0 * (size_t)channels + c];
			double s1 = in[i1 * (size_t)channels + c];
			double v  = s0 + (s1 - s0) * frac;

			if (v > 32767.0)
				v = 32767.0;
			else if (v < -32768.0)
				v = -32768.0;
			dst[i * (size_t)channels + c] = (int16_t)v;
		}
	}
	*out_frames = frames;
	return dst;
}

// Encode 16-bit PCM (`channels`, `rate`) to an Ogg/Opus stream in memory.
// Returns a malloc'd buffer in *out (caller frees) + its length, or 0 on failure.
// libsndfile's Opus encoder accepts only 8k/12k/16k/24k/48k `rate` (48000 is the
// natural choice: Opus is internally 48 kHz); an unsupported rate -- e.g. Red
// Alert's native 22050 Hz AUD score tracks (src/doors/syncconquer) -- is up-
// sampled to the nearest legal rate first (termgfx_audio_pick_opus_rate() /
// termgfx_audio_resample_linear() above) rather than falling straight back to
// the raw-PCM WAV upload, so a music track still lands in the door-side OGG
// disk cache (and gets the size win) instead of skipping it every session.
// The resample changes the ENCODED rate only: output frame count scales with
// the rate change, so wall-clock duration is unchanged, and any rate that IS
// already legal (every other caller of this function passes 48000, see
// audio_mgr.c's MIDI-render path and syncduke/syncdoom) skips the resample
// entirely and behaves exactly as before -- confirmed by inspection, this
// path never triggers for 11025 Hz Doom SFX (SFX never reaches this
// function at all, see termgfx_audio_sfx()'s use of termgfx_audio_cache_pcm()
// instead) or 48000 Hz MIDI-rendered music.
// Split out of cache_ogg so the door-side disk cache and the C;S upload share one
// encode (the Ogg bytes get written to disk AND base64'd to the client).
size_t termgfx_audio_encode_ogg(const int16_t *pcm, size_t frames, int channels,
                                int rate, double quality, uint8_t **out)
{
	struct memsink sink       = { NULL, 0, 0, 0 };
	SF_VIRTUAL_IO  vio        = { ms_filelen, ms_seek, ms_read, ms_write, ms_tell };
	SF_INFO        info;
	SNDFILE *      sf;
	int16_t *      resampled  = NULL;
	int            enc_rate   = rate;

	*out = NULL;
	if (!termgfx_audio_is_opus_rate(rate)) {
		size_t out_frames;

		enc_rate  = termgfx_audio_pick_opus_rate(rate);
		resampled = termgfx_audio_resample_linear(pcm, frames, channels, rate,
		                                          enc_rate, &out_frames);
		if (resampled == NULL)
			return 0;
		pcm    = resampled;
		frames = out_frames;
	}
	memset(&info, 0, sizeof(info));
	info.samplerate = enc_rate;
	info.channels   = channels;
	info.format     = SF_FORMAT_OGG | SF_FORMAT_OPUS;
	if (!sf_format_check(&info)) {
		free(resampled);
		return 0;
	}
	sf = sf_open_virtual(&vio, SFM_WRITE, &info, &sink);
	if (sf == NULL) {
		free(resampled);
		return 0;
	}
	// Must precede the first write.  Clamp to libsndfile's 0.0..1.0 range.
	if (quality < 0.0)
		quality = 0.0;
	if (quality > 1.0)
		quality = 1.0;
	sf_command(sf, SFC_SET_VBR_ENCODING_QUALITY, &quality, sizeof(quality));
	// Write in chunks: a single multi-million-frame sf_writef_short can crash deep
	// in the codec's analysis buffer, so feed it a bounded window at a time.
	{
		size_t off;
		for (off = 0; off < frames; ) {
			size_t     want = frames - off;
			sf_count_t got;

			if (want > 8192)
				want = 8192;
			got = sf_writef_short(sf, pcm + off * (size_t)channels, (sf_count_t)want);
			if (got <= 0)
				break;
			off += (size_t)got;
		}
	}
	sf_close(sf);                                  // flushes the OGG stream into sink
	free(resampled);
	if (sink.data == NULL || sink.len <= 0) {
		free(sink.data);
		return 0;
	}
	*out = sink.data;                              // ownership transferred to caller
	return (size_t)sink.len;
}

size_t termgfx_audio_cache_ogg(uint8_t **buf, size_t *cap, const char *file,
                               const int16_t *pcm, size_t frames,
                               int channels, int rate, double quality)
{
	uint8_t *ogg    = NULL;
	size_t   ogglen = termgfx_audio_encode_ogg(pcm, frames, channels, rate, quality, &ogg);
	size_t   n      = 0;

	if (ogglen > 0)
		n = termgfx_audio_cache_file(buf, cap, file, ogg, ogglen);
	free(ogg);
	return n;
}

#else  // !TERMGFX_WITH_SNDFILE

int termgfx_audio_have_ogg(void) { return 0; }

size_t termgfx_audio_cache_ogg(uint8_t **buf, size_t *cap, const char *file,
                               const int16_t *pcm, size_t frames,
                               int channels, int rate, double quality)
{
	(void)buf; (void)cap; (void)file; (void)pcm; (void)frames;
	(void)channels; (void)rate; (void)quality;
	return 0;
}

size_t termgfx_audio_encode_ogg(const int16_t *pcm, size_t frames, int channels,
                                int rate, double quality, uint8_t **out)
{
	(void)pcm; (void)frames; (void)channels; (void)rate; (void)quality;
	*out = NULL;
	return 0;
}

#endif // TERMGFX_WITH_SNDFILE

// Append `n` bytes to a growing malloc buffer (used by the VOC decoder).
static int pcm_append(uint8_t **buf, size_t *len, size_t *cap,
                      const uint8_t *src, size_t n)
{
	if (*len + n > *cap) {
		size_t   ncap = *cap ? *cap * 2 : 8192;
		uint8_t *nb;

		while (ncap < *len + n)
			ncap *= 2;
		nb = realloc(*buf, ncap);
		if (nb == NULL)
			return 0;
		*buf = nb;
		*cap = ncap;
	}
	memcpy(*buf + *len, src, n);
	*len += n;
	return 1;
}

size_t termgfx_audio_voc_to_pcm(const void *vocv, size_t len, uint8_t **out, int *rate)
{
	const uint8_t *voc = vocv;
	size_t         hdr, p;
	uint8_t *      pcm = NULL;
	size_t         pcmlen = 0, pcmcap = 0;
	int            rt = 11025;

	*out = NULL;
	if (len < 26 || memcmp(voc, "Creative Voice File\x1a", 20) != 0)
		return 0;
	hdr = (size_t)voc[0x14] | ((size_t)voc[0x15] << 8);
	if (hdr < 26 || hdr > len)
		hdr = 26;
	for (p = hdr; p + 4 <= len; ) {
		uint8_t        type = voc[p];
		size_t         blen;
		const uint8_t *bd;

		if (type == 0)                         // terminator
			break;
		blen = (size_t)voc[p + 1] | ((size_t)voc[p + 2] << 8) | ((size_t)voc[p + 3] << 16);
		bd   = voc + p + 4;
		if (p + 4 + blen > len)
			break;
		if (type == 1 && blen >= 2) {          // sound data: divisor, codec, then PCM
			int div = bd[0];                   // bd[1] = codec (0 = 8-bit unsigned)
			if (div < 256)
				rt = 1000000 / (256 - div);
			if (!pcm_append(&pcm, &pcmlen, &pcmcap, bd + 2, blen - 2)) {
				free(pcm);
				return 0;
			}
		}
		else if (type == 2) {                  // continuation block: PCM only
			if (!pcm_append(&pcm, &pcmlen, &pcmcap, bd, blen)) {
				free(pcm);
				return 0;
			}
		}
		// types 3 (silence) / 6,7 (repeat) / 8 (extended) / 9 (new-format) are
		// rare in Duke SFX -- skipped; the play-once concatenation above suffices.
		p += 4 + blen;
	}
	if (pcm == NULL || pcmlen == 0) {
		free(pcm);
		return 0;
	}
	*out  = pcm;
	*rate = rt;
	return pcmlen;
}

size_t termgfx_audio_load(uint8_t **buf, size_t *cap, int slot, const char *file)
{
	size_t need = strlen(file) + 48;

	if (need > *cap) {
		*buf = realloc(*buf, need);
		*cap = need;
	}
	return (size_t)sprintf((char *)*buf,
	                       "\x1b_SyncTERM:A;Load;S=%d;%s\x1b\\", slot, file);
}

size_t termgfx_audio_queue(uint8_t **buf, size_t *cap, int ch, int slot,
                           int vol, int pan, int loop)
{
	int vl, vr;

	if (vol < 0)
		vol = 0;
	if (vol > 100)
		vol = 100;
	if (pan < -100)
		pan = -100;
	if (pan > 100)
		pan = 100;
	vl = (pan > 0) ? vol * (100 - pan) / 100 : vol;
	vr = (pan < 0) ? vol * (100 + pan) / 100 : vol;

	if (*cap < 96) {
		*buf = realloc(*buf, 96);
		*cap = 96;
	}
	return (size_t)sprintf((char *)*buf,
	                       "\x1b_SyncTERM:A;Queue;C=%d;S=%d;VL=%d;VR=%d%s\x1b\\",
	                       ch, slot, vl, vr, loop ? ";L" : "");
}

size_t termgfx_audio_volume(uint8_t **buf, size_t *cap, int ch, int vol)
{
	if (vol < 0)
		vol = 0;
	if (vol > 100)
		vol = 100;
	if (*cap < 64) {
		*buf = realloc(*buf, 64);
		*cap = 64;
	}
	return (size_t)sprintf((char *)*buf,
	                       "\x1b_SyncTERM:A;Volume;C=%d;V=%d\x1b\\", ch, vol);
}

size_t termgfx_audio_volume_lr(uint8_t **buf, size_t *cap, int ch, int vl, int vr)
{
	if (vl < 0)
		vl = 0;
	if (vl > 100)
		vl = 100;
	if (vr < 0)
		vr = 0;
	if (vr > 100)
		vr = 100;
	if (*cap < 64) {
		*buf = realloc(*buf, 64);
		*cap = 64;
	}
	return (size_t)sprintf((char *)*buf,
	                       "\x1b_SyncTERM:A;Volume;C=%d;VL=%d;VR=%d\x1b\\", ch, vl, vr);
}

size_t termgfx_audio_synth(uint8_t **buf, size_t *cap, int slot,
                           const char *shape, int freq, int ms)
{
	size_t need = strlen(shape) + 64;

	if (need > *cap) {
		*buf = realloc(*buf, need);
		*cap = need;
	}
	return (size_t)sprintf((char *)*buf,
	                       "\x1b_SyncTERM:A;Synth;S=%d;W=%s;F=%d;T=%d\x1b\\",
	                       slot, shape, freq, ms);
}

size_t termgfx_audio_flush(uint8_t **buf, size_t *cap, int ch, int fade_ms)
{
	if (*cap < 64) {
		*buf = realloc(*buf, 64);
		*cap = 64;
	}
	if (fade_ms > 0)
		return (size_t)sprintf((char *)*buf,
		                       "\x1b_SyncTERM:A;Flush;C=%d;O=%d\x1b\\", ch, fade_ms);
	return (size_t)sprintf((char *)*buf,
	                       "\x1b_SyncTERM:A;Flush;C=%d\x1b\\", ch);
}

size_t termgfx_audio_update(uint8_t **buf, size_t *cap, int ch)
{
	size_t   need = 48;
	uint8_t *p;

	if (need > *cap) {
		*buf = realloc(*buf, need);
		*cap = need;
	}
	p  = *buf;
	p += sprintf((char *)p, "\x1b_SyncTERM:A;Update;C=%d\x1b\\", ch);
	return (size_t)(p - *buf);
}
