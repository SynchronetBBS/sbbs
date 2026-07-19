/* test_termgfx_termio_audio_ini_headroom.c -- the pre-encode headroom key reaches the
 * ENCODER, and is not merely parsed.
 *
 * "[audio] headroom" is the door's answer to the battle-scene crunch: it scales
 * the mixer's PCM before the Opus encode so the codec's ringing has somewhere to
 * land instead of overshooting full scale and wrapping to the wrong sign (see
 * SST_AUDIO_HEADROOM in termgfx_termio.h). Everything about it lives upstream of the
 * wire, which makes it exactly the kind of knob that rots into a no-op without
 * any test noticing: delete the audio_apply_headroom() call in
 * termgfx_termio_audio_stream() and every other audio test in this directory still
 * passes, because chunk counts, ordering, backlog and flush behavior are all
 * unchanged. Only the sample VALUES move.
 *
 * So this test reads the sample values, which means decoding the wire. That is
 * the only honest observable, and the cheaper ones were tried and rejected: the
 * ENCODED BYTE COUNT does not work, because libsndfile maps Opus quality
 * ~linearly to bitrate (see termgfx/audio.h) rather than letting it follow the
 * signal -- a full-scale square wave and the same wave at 1% of full scale
 * measured 22964 vs 22708 bytes, a 1% difference that no threshold can separate.
 * A test asserting on that would have passed at every headroom, including a
 * headroom that never ran.
 *
 * The proof: feed a full-scale 300Hz sine, decode the A;LoadBlob payload the
 * door actually put on the wire, and assert the decoded PEAK. A sine is chosen
 * over the square wave the defect involves precisely because Opus reproduces it
 * without ringing, so the peak is the scaling factor and nothing else. Seeded
 * with "headroom = 50" the peak must land near 32767 * 0.50 = 16384; the
 * SST_AUDIO_HEADROOM default of 70 would put it at ~22900 and an un-applied
 * headroom at ~32700, both far outside the asserted band. Unsatisfiable unless
 * the ini value displaced the default AND the scaling actually ran.
 *
 * Needs a real syncscumm.ini in CWD; unit_termgfx_termio.sh runs this binary from a
 * temp directory seeded with the key. Its own binary because termgfx_termio keeps
 * file-static session state with no reset. cc'd + run by unit_termgfx_termio.sh.
 */
#include "termgfx_termio.h"

/* Mirrors termgfx_termio.c's own internal SST_AUDIO_RATE default (24000) --
 * termgfx_termio.h no longer exposes it as a public macro the way
 * door/sst_io.h once did, so the test keeps its own copy rather than guess a
 * literal. */
#define SST_AUDIO_RATE 24000

#include <assert.h>
#include <math.h>
#include <sndfile.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int drain(int fd, char *buf, size_t cap)
{
	ssize_t n;
	size_t  tot = 0;

	while ((n = recv(fd, buf + tot, cap - 1 - tot, MSG_DONTWAIT)) > 0) {
		tot += (size_t)n;
		if (tot >= cap - 1)
			break;
	}
	buf[tot] = '\0';
	return (int)tot;
}

/* Minimal base64 decoder. Its own rather than a library's because termgfx's
 * b64_encode() is file-static to audio.c and xpdev exposes no decoder -- and a
 * test that reads the wire needs to undo exactly what the wire did to it. */
static size_t b64_dec(const char *in, size_t len, unsigned char *out)
{
	static const char *tbl =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	size_t   i, n = 0;
	uint32_t acc  = 0;
	int      bits = 0;

	for (i = 0; i < len; i++) {
		const char *p = strchr(tbl, in[i]);

		if (in[i] == '=' || p == NULL)
			break;
		acc   = (acc << 6) | (uint32_t)(p - tbl);
		bits += 6;
		if (bits >= 8) {
			bits -= 8;
			out[n++] = (unsigned char)((acc >> bits) & 0xff);
		}
	}
	return n;
}

/* libsndfile virtual I/O over the decoded Ogg, so the test decodes the exact
 * bytes the door emitted with the exact library SyncTERM decodes them with. */
struct msrc { const unsigned char *d; sf_count_t len, pos; };
static sf_count_t ms_len(void *u) { return ((struct msrc *)u)->len; }
static sf_count_t ms_seek(sf_count_t off, int whence, void *u)
{
	struct msrc *m = (struct msrc *)u;

	if (whence == SEEK_SET)
		m->pos = off;
	else if (whence == SEEK_CUR)
		m->pos += off;
	else
		m->pos = m->len + off;
	return m->pos;
}
static sf_count_t ms_read(void *ptr, sf_count_t n, void *u)
{
	struct msrc *m = (struct msrc *)u;

	if (n > m->len - m->pos)
		n = m->len - m->pos;
	memcpy(ptr, m->d + m->pos, (size_t)n);
	m->pos += n;
	return n;
}
static sf_count_t ms_tell(void *u) { return ((struct msrc *)u)->pos; }

int main(void)
{
	int         sv[2];
	char        fdarg[32];
	char       *argv[3];
	static char out[1 << 21];
	int         peak = 0;

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
	snprintf(fdarg, sizeof fdarg, "-s%d", sv[1]);
	argv[0] = (char *)"test_termgfx_termio_audio_ini_headroom";
	argv[1] = fdarg;
	argv[2] = NULL;

	assert(termgfx_termio_init(2, argv) == 1);
	assert(termgfx_termio_active() == 1);
	termgfx_termio_flush();
	drain(sv[0], out, sizeof out);

	/* Sixel-capable DA1 + a SyncTERM CTDA reporting cterm 1.330 (the same
	 * replies test_termgfx_termio.c plays): >= TERMGFX_CTERM_VER_BLOB, which is what
	 * puts the encoded chunk INLINE in an A;LoadBlob rather than in a cache
	 * file -- the production path for any current SyncTERM, and the one whose
	 * payload this test can read straight off the socket. Then the digital-
	 * audio caps reply + a JXL "no" to latch the settle flag, as in
	 * test_termgfx_termio_audio.c. */
	{
		const char *replies = "\x1b[?63;4c" "\x1b[=67;84;101;114;109;1;330c"
		                      "\x1b[24;80R" "\x1b[=7;100;1n" "\x1b[=1;0n";

		assert(send(sv[0], replies, strlen(replies), 0) > 0);
		termgfx_termio_pump();
	}
	assert(termgfx_termio_audio_available() == 1);

	{
		static int16_t pcm[SST_AUDIO_RATE * 2];   /* 1s of stereo */
		int            i;

		/* Full-scale 300Hz sine. 32767, not 32768: the point is a clean peak
		 * the codec can return unchanged, and this test is measuring the
		 * scaling, not the rail behavior the knob exists to avoid. */
		for (i = 0; i < SST_AUDIO_RATE; i++) {
			int16_t v = (int16_t)(32767.0 * sin(2.0 * M_PI * 300.0 * i / SST_AUDIO_RATE));

			pcm[2 * i]     = v;
			pcm[2 * i + 1] = v;
		}
		termgfx_termio_flush();
		drain(sv[0], out, sizeof out);   /* clear the caps handshake */
		termgfx_termio_audio_stream(pcm, SST_AUDIO_RATE);
		termgfx_termio_flush();
		drain(sv[0], out, sizeof out);
	}

	/* Pull the first A;LoadBlob's base64 payload -- "...;S=<slot>;<b64>ESC\" --
	 * and decode it. The FIRST is enough: the headroom is applied per block, so
	 * if it reached the encoder at all it reached this chunk. */
	{
		static unsigned char ogg[1 << 20];
		static short         dec[SST_AUDIO_RATE * 2];
		const char          *blob = strstr(out, "A;LoadBlob;");
		const char          *b64, *end;
		size_t               n;
		struct msrc          m;
		SF_VIRTUAL_IO        vio = { ms_len, ms_seek, ms_read, NULL, ms_tell };
		SF_INFO              info;
		SNDFILE             *sf;
		sf_count_t           got;
		int                  i;

		assert(blob != NULL);
		b64 = strchr(blob + strlen("A;LoadBlob;"), ';');   /* past "S=<slot>" */
		assert(b64 != NULL);
		b64++;
		end = strstr(b64, "\x1b\\");                       /* the APC's ST */
		assert(end != NULL);
		n = b64_dec(b64, (size_t)(end - b64), ogg);
		assert(n > 0 && n < sizeof ogg);

		m.d = ogg; m.len = (sf_count_t)n; m.pos = 0;
		memset(&info, 0, sizeof info);
		sf = sf_open_virtual(&vio, SFM_READ, &info, &m);
		assert(sf != NULL);
		got = sf_readf_short(sf, dec, SST_AUDIO_RATE);
		sf_close(sf);
		assert(got > 0);
		for (i = 0; i < (int)got * info.channels; i++)
			if (abs(dec[i]) > peak)
				peak = abs(dec[i]);
	}

	/* 32767 * 0.50 = 16384, and Opus returns this sine within a couple of
	 * percent. The band is deliberately wider than that and still cannot be
	 * reached by any other headroom: the SST_AUDIO_HEADROOM default of 70 puts
	 * the peak at ~22900 and a headroom that never ran at ~32700. That gap is
	 * the whole test -- it proves the seeded VALUE was used, not merely that
	 * some scaling happened. */
	assert(peak > 14000 && peak < 19000);

	printf("SST_IO_AUDIO_INI_HEADROOM OK (decoded peak %d at headroom = 50)\n", peak);
	return 0;
}
