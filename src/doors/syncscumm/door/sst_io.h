/* sst_io.h -- SyncSCUMM's terminal-session layer (pure C, no ScummVM).
 * Modeled on syncconquer/door/door_io.c; termgfx supplies the encoders,
 * probe parsers, pacing primitives and control strings -- the session,
 * tier dispatch and present loop live here. */
#ifndef SYNCSCUMM_SST_IO_H_
#define SYNCSCUMM_SST_IO_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SST_FB_W 320
#define SST_FB_H 200

int  sst_io_init(int argc, char **argv);
void sst_io_shutdown(void);
int  sst_io_active(void);
void sst_io_pump(void);
void sst_io_flush(void);
int  sst_io_quit_requested(void);
int  sst_io_hung_up(void);   /* peer EOF, or a hard read/write error -- see sst_io.c */

/* Was argv[idx] one sst_io_init() resolved itself (-s<fd>, a DOOR32.SYS
 * path)? main() uses this to strip door-only argv entries before handing
 * the rest to scummvm_main(), which rejects options it doesn't know. */
int  sst_io_consumed(int idx);

/* parser-state probes (unit test + stats bar) */
int  sst_io_have_sixel(void);
int  sst_io_is_syncterm(void);
int  sst_io_jxl_supported(void);
int  sst_io_stats_visible(void);

/* Does this session have working audio? Drives the subtitles auto-decision
 * (door/syncscumm.cpp): no audio -> subtitles on. M2 has no audio path at
 * all, so this unconditionally returns 0 for now; M4 (chunked mixer
 * streaming over the audio APCs) will wire real per-session detection --
 * e.g. the same libsndfile-probe-or-silent-tier gate DESIGN.md's Audio path
 * section describes -- in its place. */
int  sst_io_audio_available(void);

/* Present one changed frame: idx is SST_FB_W*SST_FB_H palette indices,
 * pal768 is 256 RGB triples (8-bit, no <<2 scaling). A no-op off a terminal
 * session (sst_io_active() false) or a dead one (sst_io_hung_up()). In
 * SYNCSCUMM_SIXELOUT capture mode, appends a self-contained full-res sixel
 * frame per call (no pacing/dedupe), capped at 200 frames. */
void sst_io_present(const uint8_t *idx, const uint8_t *pal768);

/* Frames whose staged bytes were dropped because out_put()'s 256KB stage
 * buffer stayed full after a flush attempt (dead-peer backpressure, not a
 * hard error) -- see sst_io.c's out_put(). Test-introspection / stats. */
unsigned sst_io_frames_dropped(void);

#ifdef __cplusplus
}
#endif

#endif /* SYNCSCUMM_SST_IO_H_ */
