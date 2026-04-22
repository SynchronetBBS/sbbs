/*
 * sndfile.h — syncterm-local libsndfile wrapper.
 *
 * Three build modes, mutually exclusive:
 *
 *   WITHOUT_SNDFILE  — stubs only; sndfile_available() returns false
 *                      unconditionally, sndfile_decode() always fails.
 *   WITH_SNDFILE     — runtime dlopen via xp_dlopen (xpdev).  The
 *                      first call to sndfile_available() attempts the
 *                      library load.  No link-time dependency on
 *                      libsndfile.
 *   STATIC_SNDFILE   — direct linkage to libsndfile; the binary
 *                      carries it (used for macOS / Win32 release
 *                      shape).  sndfile_available() is always true.
 *
 * The decode path normalizes output to S16 stereo 44100 Hz regardless
 * of the source file's native format (resampled and channel-remapped
 * internally).  Caller takes ownership of the returned `*out_frames`
 * buffer and must free() it.
 */
#ifndef _SYNCTERM_SNDFILE_H_
#define _SYNCTERM_SNDFILE_H_

#include <gen_defs.h>   /* bool, int16_t, uint32_t, size_t */

#ifdef __cplusplus
extern "C" {
#endif

/* Probe libsndfile availability.
 *
 * WITH_SNDFILE: lazy-loads the library on first call.  Returns true if
 * the dlopen + dlsym for required symbols all succeed.  Caches the
 * result; subsequent calls are O(1).
 * STATIC_SNDFILE: always true.
 * WITHOUT_SNDFILE: always false.
 */
bool sndfile_available(void);

/* Decode `path` into a newly malloc'd S16 stereo 44100 Hz buffer.
 * On success: writes the buffer pointer to *out_frames, frame count to
 * *out_nframes, and returns true.  Caller owns *out_frames and must
 * free() it.
 * On failure (file not found, bad format, unsupported sample type,
 * libsndfile not available, allocation failure): returns false with
 * *out_frames = NULL and *out_nframes = 0.
 */
bool sndfile_decode(const char *path, int16_t **out_frames, size_t *out_nframes);

#ifdef __cplusplus
}
#endif

#endif /* _SYNCTERM_SNDFILE_H_ */
