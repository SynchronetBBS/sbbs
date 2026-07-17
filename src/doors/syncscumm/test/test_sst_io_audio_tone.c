/* test_sst_io_audio_tone.c -- the tone-only audio tier reads as NO audio.
 *
 * A terminal with the audio APC but no libsndfile (caps reply
 * "ESC[=7;100;0n") can only synth tones: A;Load is a silent no-op there, so
 * queueing a streamed slot would play an empty buffer -- silent, and worse
 * than silent, because the subtitles auto-decision would have turned
 * subtitles off. It must resolve to "no audio", and it must never be asked
 * the Opus format query (there is no libsndfile to answer it).
 *
 * Its own binary because sst_io keeps file-static session state with no
 * reset, so a second tier needs a fresh process. cc'd + run by
 * unit_sst_io.sh.
 */
#include "sst_io.h"

#include <assert.h>
#include <stdio.h>
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

int main(void)
{
	int         sv[2];
	char        fdarg[32];
	char       *argv[3];
	static char out[65536];

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
	snprintf(fdarg, sizeof fdarg, "-s%d", sv[1]);
	argv[0] = (char *)"test_sst_io_audio_tone";
	argv[1] = fdarg;
	argv[2] = NULL;

	assert(sst_io_init(2, argv) == 1);
	assert(sst_io_active() == 1);
	sst_io_flush();
	drain(sv[0], out, sizeof out);
	assert(strstr(out, "libsndfile") != NULL);

	/* Tone-only caps reply, plus a JXL "no" (ESC[=1;0n) to latch the
	 * graphics settle flag so this test eats none of SST_GFX_SETTLE_MS in
	 * real time. */
	{
		const char *reply = "\x1b[=7;100;0n\x1b[=1;0n";
		assert(send(sv[0], reply, strlen(reply), 0) > 0);
		sst_io_pump();
	}

	assert(sst_io_audio_available() == 0);

	/* ... and the tone-only tier was never asked the Opus query. */
	sst_io_flush();
	drain(sv[0], out, sizeof out);
	assert(strstr(out, "32;100") == NULL);

	/* The same PCM on a tone-only terminal must produce ZERO audio bytes. */
	{
		static int16_t pcm[SST_AUDIO_RATE * 2];
		int            i;

		for (i = 0; i < SST_AUDIO_RATE; i++)
			pcm[2 * i] = pcm[2 * i + 1] = (int16_t)(i * 37);
		drain(sv[0], out, sizeof out);   /* clear */
		sst_io_audio_stream(pcm, SST_AUDIO_RATE);
		sst_io_flush();
		drain(sv[0], out, sizeof out);
		assert(strstr(out, "A;") == NULL);
	}

	printf("SST_IO_AUDIO_TONE OK\n");
	return 0;
}
