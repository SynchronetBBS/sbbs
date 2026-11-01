/* Headless session-audio teardown regression using the real mixer. */
#include "audio_apc.h"
#include "conn.h"
#include "term.h"
#include <xpbeep.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Device callbacks use this internal mixer entry point, as does mixertest. */
size_t xp_mixer_pull(int16_t *out, size_t frames);

struct cterminal *cterm;
static unsigned failures;
static unsigned device_refs;
static unsigned device_closes;
static unsigned notifications;

#define CHECK(e) do { if (!(e)) { \
	fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #e); \
	failures++; \
} } while (0)

/* Renamed only in this target: no physical audio device is opened. */
bool
xptone_open(void)
{
	device_refs++;
	return true;
}

bool
xptone_close(void)
{
	int16_t samples[16];

	/* A real device close drains every stream. All session streams must
	 * already be empty before releasing even the first device reference. */
	CHECK(xp_mixer_pull(samples, 8) == 0);
	CHECK(device_refs > 0);
	if (device_refs > 0)
		device_refs--;
	device_closes++;
	return true;
}

int
conn_send(const void *buffer, size_t length, unsigned int timeout)
{
	(void)buffer;
	(void)timeout;
	notifications++;
	return (int)length;
}

void
cterm_respond(struct cterminal *terminal, const char *data, size_t length)
{
	(void)terminal;
	(void)data;
	(void)length;
}

int
clean_path(char *path, size_t size)
{
	(void)path;
	(void)size;
	CHECK(false); /* This test synthesizes audio; it never reads files. */
	return 0;
}

static void
command(const char *text)
{
	char buffer[128];
	CHECK(strlen(text) < sizeof(buffer));
	strcpy(buffer, text);
	audio_apc_handler(buffer, strlen(buffer), "", NULL);
}

static xp_audio_handle_t
queued_alias(bool loop)
{
	xp_audio_opts_t opts = XP_AUDIO_OPTS_INIT;
	int16_t *frames = malloc(256 * XPBEEP_FRAMESIZE);
	xp_audio_handle_t handle;

	CHECK(frames != NULL);
	if (frames == NULL)
		return -1;
	for (size_t i = 0; i < 256 * 2; i++)
		frames[i] = 1000;
	CHECK(xptone_open());
	handle = xp_audio_open(-12.0f, -12.0f);
	CHECK(handle >= 0);
	opts.loop = loop;
	CHECK(xp_audio_append(handle, frames, 256, &opts));
	CHECK(!xp_audio_is_idle(handle));
	return handle;
}

static void
run_session(bool aliases, bool all_channels)
{
	struct cterminal terminal = {0};
	struct cterminal status = {0};
	char buffer[128];
	int16_t samples[16];
	unsigned channels = all_channels ? 14 : 2;
	unsigned alias_refs = aliases ? (all_channels ? 4 : 2) : 0;
	unsigned closes_before = device_closes;

	terminal.music_stream = -1;
	terminal.fx_stream = -1;
	status.music_stream = -1;
	status.fx_stream = -1;
	cterm = &terminal;
	if (aliases) {
		terminal.music_stream = queued_alias(false);
		terminal.fx_stream = queued_alias(true);
		command("Update;C=0");
		command("Update;C=1");
		if (all_channels) {
			terminal.status_sub = &status;
			status.music_stream = queued_alias(true);
			status.fx_stream = queued_alias(false);
		}
	}
	for (unsigned i = 0; i < channels; i++) {
		unsigned channel = all_channels ? i + 2 : (i == 0 ? 2 : 15);
		command("Synth;S=0;W=SIN;F=440;T=256f");
		snprintf(buffer, sizeof(buffer), "Queue;C=%u;S=0%s", channel,
		    (i % 2 == 0) ? ";L" : "");
		command(buffer);
		snprintf(buffer, sizeof(buffer), "Update;C=%u", channel);
		command(buffer);
	}
	/* Leave an unqueued patch to exercise patch-table cleanup too. */
	command("Synth;S=1;W=SIN;F=220;T=128f");
	audio_apc_poll(cterm);
	CHECK(xp_mixer_pull(samples, 8) == 8);

	audio_apc_cleanup(cterm);
	CHECK(device_closes - closes_before == channels);
	CHECK(device_refs == alias_refs);
	CHECK(xp_mixer_pull(samples, 8) == 0);
	CHECK(xp_audio_is_idle(terminal.music_stream));
	CHECK(xp_audio_is_idle(terminal.fx_stream));
	CHECK(xp_audio_is_idle(status.music_stream));
	CHECK(xp_audio_is_idle(status.fx_stream));

	/* Teardown must disarm notifications, including cterm-owned channels. */
	audio_apc_poll(cterm);
	CHECK(notifications == 0);
	/* A slot left loaded in the previous session must now be empty. */
	command("Queue;C=2;S=1");
	CHECK(xp_mixer_pull(samples, 8) == 0);
	CHECK(device_refs == alias_refs);

	/* Repeated cleanup is safe; cterm still owns its two stream handles. */
	audio_apc_cleanup(cterm);
	CHECK(device_closes - closes_before == channels);
	for (struct cterminal *node = &terminal; node; node = node->status_sub) {
		if (node->music_stream >= 0) {
			xp_audio_close(node->music_stream);
			CHECK(xptone_close());
		}
		if (node->fx_stream >= 0) {
			xp_audio_close(node->fx_stream);
			CHECK(xptone_close());
		}
	}
	CHECK(device_refs == 0);
	cterm = NULL;
}

int
main(void)
{
	xpbeep_sound_devices_enabled = 0;
	audio_apc_cleanup(NULL);
	CHECK(device_closes == 0);
	run_session(true, true);
	run_session(false, false);
	run_session(true, false);
	audio_apc_cleanup(NULL);
	if (failures != 0)
		return EXIT_FAILURE;
	puts("audio_apc_test: session audio stopped before device close");
	return EXIT_SUCCESS;
}
