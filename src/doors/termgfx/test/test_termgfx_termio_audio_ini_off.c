/* test_termgfx_termio_audio_ini_off.c -- the sysop "[audio] enabled = false" key
 * produces ZERO audio bytes AND reports "no audio this session", on a
 * terminal that CAN play them -- and, just as importantly, does that
 * WITHOUT re-reading termgfx_test.ini on every PCM block for the rest of the
 * session.
 *
 * The distinction that makes this test worth its own binary: the tone-only
 * case (test_termgfx_termio_audio_tone.c) is silent because the TERMINAL cannot
 * decode a streamed chunk. This one is silent because the SYSOP said so --
 * the terminal here answers the digital-tier caps reply and would happily
 * play everything we sent. Turning sound off must therefore cost the uplink
 * nothing at all, rather than shipping a stream nobody hears; that is the
 * whole reason the ini text tells sysops to spell a mute as
 * "enabled = false" and never "volume = 0".
 *
 * The re-read regression this also guards: termgfx_termio_audio_stream() used to
 * retry audio_stream_open() (and therefore audio_read_ini() -> fopen() +
 * iniReadFile() + strListFree() of termgfx_test.ini) every single time it was
 * called with g_stream still NULL -- which, with audio disabled,
 * termgfx_stream_create() is guaranteed to keep returning, so g_stream never
 * latches and NOTHING ever stops the retries. The real caller is this door's
 * mixer tick (audio_term.cpp's tick() via pollEvent()), so that is tens to
 * hundreds of INI re-reads per second for the whole session -- and this
 * door's startup_dir sits under a CIFS-mounted /sbbs, so every one of those
 * is a real SMB open/read/close round trip: the exact shape this host has
 * chased before as "slow disk access" and found to be an open/close storm.
 * A single PCM block (the pre-fix version of this test) cannot see that --
 * one call looks identical whether the fix bails before the retry or after
 * it. Feeding MANY blocks and counting fopen("...termgfx_test.ini") calls
 * across all of them is what actually distinguishes "opened once, ever"
 * from "opened once per block".
 *
 * Made observable via a link-time --wrap of fopen() (see unit_termgfx_termio.sh's
 * build line for this binary): __wrap_fopen() counts every open whose path
 * contains "termgfx_test.ini" and forwards to the real fopen() via
 * __real_fopen(), so termgfx_termio.c's behavior (including the two real ini reads
 * it does -- termgfx_read_ini() at init, audio_read_ini() from the stream-open
 * path) is otherwise completely unchanged. This was chosen over deleting or
 * chmod-000'ing the ini file after init because that would only prove "a
 * failed re-read doesn't crash", not "no re-read was attempted" -- with
 * audio disabled, a fopen() failure and a fopen() success both end in the
 * same guaranteed-NULL stream, so that approach cannot tell a fixed build
 * from a broken one. Counting the actual calls can.
 *
 * Like test_termgfx_termio_sixelmax_override.c, this one needs a real termgfx_test.ini
 * in CWD when the door reads it, so unit_termgfx_termio.sh runs this binary from a
 * dedicated temp directory seeded with the key rather than from $HERE/$DOOR.
 *
 * Its own binary because termgfx_termio keeps file-static session state with no
 * reset. cc'd + run by unit_termgfx_termio.sh.
 */
#include "termgfx_termio.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

/* --wrap=fopen (unit_termgfx_termio.sh, this binary only): every fopen() call in
 * termgfx_termio.c is redirected here. Only "termgfx_test.ini" opens are counted --
 * the door also fopen()s a trace/tee/capture path off an env var elsewhere,
 * and those are not what this test is about. */
extern FILE *__real_fopen(const char *path, const char *mode);

static int g_ini_fopen_calls = 0;

FILE *__wrap_fopen(const char *path, const char *mode)
{
	if (path != NULL && strstr(path, "termgfx_test.ini") != NULL)
		g_ini_fopen_calls++;
	return __real_fopen(path, mode);
}

static uint32_t ms_now(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return (uint32_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

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

int main(void)
{
	int         sv[2];
	char        fdarg[32];
	char       *argv[3];
	static char out[65536];
	int         base_fopen_calls;

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
	snprintf(fdarg, sizeof fdarg, "-s%d", sv[1]);
	argv[0] = (char *)"test_termgfx_termio_audio_ini_off";
	argv[1] = fdarg;
	argv[2] = NULL;

	/* Pin the name the ini file is looked up under: this binary is a
	 * test, not a door, so argv[0] is no basis for one. */
	termgfx_termio_set_app_name("termgfx_test");
	assert(termgfx_termio_init(2, argv) == 1);
	assert(termgfx_termio_active() == 1);
	termgfx_termio_flush();
	drain(sv[0], out, sizeof out);

	/* termgfx_termio_init() -> termgfx_read_ini() opens termgfx_test.ini exactly once, to
	 * latch g_sixel_max_override and g_audio_enabled. That is the ONE
	 * legitimate read before any PCM has flowed; everything from here on
	 * is measured as a delta off this baseline. */
	base_fopen_calls = g_ini_fopen_calls;
	assert(base_fopen_calls == 1);

	/* Asked BEFORE any caps reply has landed, deliberately: with sound off
	 * the answer cannot depend on what the terminal says, so the question
	 * must be answered immediately rather than blocking out the capability
	 * settle window waiting for a reply the door has already decided to
	 * ignore. Whole-session budget at boot, so a whole second of it spent on
	 * a foregone conclusion is worth a gate. */
	{
		uint32_t t0 = ms_now();

		assert(termgfx_termio_audio_available() == 0);
		assert(ms_now() - t0 < 200);
	}

	/* A digital-audio caps reply (ESC[=7;100;1n) -- this terminal CAN play a
	 * streamed chunk -- plus a JXL "no" (ESC[=1;0n) to latch the graphics
	 * settle flag so this test eats none of TERMGFX_GFX_SETTLE_MS in real time. */
	{
		const char *reply = "\x1b[=7;100;1n\x1b[=1;0n";
		assert(send(sv[0], reply, strlen(reply), 0) > 0);
		termgfx_termio_pump();
	}

	/* Still 0 even now that the terminal has confirmed the digital tier, and
	 * THIS is the assertion that keeps a talkie's dialogue on screen: the
	 * question is "will this player hear anything", not "could this terminal
	 * play something". A sysop who turns the stream off on a SyncTERM used
	 * to get an answer of 1 here -- so subtitles on "auto" resolved OFF, and
	 * a CD talkie's speech was neither heard (sound is off) nor seen (the
	 * terminal looked capable). Answering honestly routes the sysop's switch
	 * through the existing auto decision with no reordering. */
	assert(termgfx_termio_audio_available() == 0);

	/* The stream opens lazily on the first PCM, which is where the ini is
	 * read -- so the byte-level assertion below is what proves the key was
	 * honored for a single block, same as before this test grew a second
	 * case. Feed MANY blocks below (not one) to prove the re-read storm is
	 * gone, not just that one block stayed silent. */
	{
		/* An arbitrary block of non-silent stereo PCM -- deliberately NOT
		 * tied to the chunk length, unlike the other audio tests: audio is
		 * OFF here, so what this proves is that NOTHING ships, at any
		 * block size. Only the block COUNT below carries meaning. */
		static int16_t pcm[2205 * 2];
		int            i, block;

		for (i = 0; i < 2205; i++) {
			pcm[2 * i]     = (int16_t)(i * 37);
			pcm[2 * i + 1] = (int16_t)(i * -37);
		}
		termgfx_termio_flush();
		drain(sv[0], out, sizeof out);   /* clear the caps handshake */

		/* 50 blocks (~5s of mixer ticks at a realistic 100ms chunk size) --
		 * enough that a per-block INI re-read is unmistakable rather than a
		 * one-off that could be dismissed as noise. Every one of these, in
		 * the real door, is a separate pollEvent()-driven tick(), i.e. a
		 * separate opportunity for the pre-fix code to re-open the file. */
		for (block = 0; block < 50; block++)
			termgfx_termio_audio_stream(pcm, 2205);

		termgfx_termio_flush();
		drain(sv[0], out, sizeof out);
		/* ZERO audio bytes -- not "no A;Queue". Nothing audio-shaped at all
		 * may reach the wire once the sysop has turned sound off: no Queue,
		 * no Load, no Volume, no cache churn. With the same feed and the
		 * key absent, test_termgfx_termio_audio.c asserts >= 7 A;Queue APCs here. */
		assert(strstr(out, "A;") == NULL);

		/* THE regression assertion: 50 blocks fed to a permanently-disabled
		 * stream must not have opened termgfx_test.ini even once more than the
		 * single init-time read above. Pre-fix, each of the 50 calls above
		 * hit g_stream == NULL, retried audio_stream_open(), and therefore
		 * audio_read_ini()'s fopen() -- so this would read 51, not 1. */
		assert(g_ini_fopen_calls == base_fopen_calls);
	}

	printf("TERMGFX_TERMIO_AUDIO_INI_OFF OK\n");
	return 0;
}
