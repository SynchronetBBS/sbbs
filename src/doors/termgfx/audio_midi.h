#ifndef TERMGFX_AUDIO_MIDI_H_
#define TERMGFX_AUDIO_MIDI_H_

#include <stddef.h>
#include <stdint.h>

// audio_midi.h -- render a MIDI/MUS/XMI song (in memory) to interleaved S16
// stereo PCM via the vendored libADLMIDI (OPL3 / Nuked -- authentic FM). For
// game music over the SyncTERM audio APC: the door reads the song bytes (Duke
// .MID from the GRP, Doom MUS lump), renders one non-looping pass here, and
// ships the PCM through termgfx_audio_music() (which loops it on the terminal).
// One synth, shared by both doors -- the only per-game part is getting the bytes.

// Render `midi`/`len` (SMF/MUS/XMI, auto-detected) to malloc'd interleaved S16
// stereo PCM at `rate` Hz. On success returns 1 and sets *out (caller frees with
// free()) and *nframes (stereo frame count). Returns 0 on failure (*out = NULL).
// Capped at `max_ms` (0 = built-in default) so a long/looping song can't blow up
// memory -- the result is meant to be looped by the player, not played whole.
int termgfx_midi_render(const void *midi, size_t len, int rate,
                        uint32_t max_ms, int16_t **out, size_t *nframes);

#endif // TERMGFX_AUDIO_MIDI_H_
