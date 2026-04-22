/*
 * audio_apc.h — BBS-facing audio APC (Application Program Command)
 * dispatcher: SyncTERM:A;* verbs, SyncTERM:Q; feature query, and the
 * CSI = 7 n DSR response.  Owns a 256-slot patch table (decoded /
 * synthesized PCM buffers) and 16 mixer channels (Ch=0 aliases
 * cterm->music_stream, Ch=1 aliases cterm->fx_stream, Ch=2..15 are
 * APC-dedicated and lazy-opened at -12 dB base).
 *
 * Interface to term.c (wires these into apc_handler + the cterm
 * ext_state_7 callback + the periodic idle-transition poll).
 */
#ifndef _SYNCTERM_AUDIO_APC_H_
#define _SYNCTERM_AUDIO_APC_H_

#include <gen_defs.h>
#include <cterm.h>

#ifdef __cplusplus
extern "C" {
#endif

/* APC verb dispatcher.  `strbuf` is the APC payload AFTER the
 * "SyncTERM:A;" prefix; `slen` is its length.  `fn` is the cache
 * directory root (same convention as the JXL handler). */
void audio_apc_handler(char *strbuf, size_t slen, char *fn, void *apcd);

/* Feature-probe dispatcher for APC SyncTERM:Q;<feature> ST.  `strbuf`
 * and `slen` point PAST the "SyncTERM:Q;" prefix. */
void feature_query_handler(char *strbuf, size_t slen, void *apcd);

/* Callback registered into cterm->ext_state_7_cb.  Emits the CSI = 7
 * DSR response based on the query sub-parameters (query-all when
 * nparams == 1, query-one when nparams == 2). */
void audio_ext_state_7(struct cterminal *cterm, int nparams,
                       const uint64_t *params);

/* Called once per input-servicing tick from term.c's read loop.  Walks
 * the per-channel Update-armed flags, notices any running→stopped
 * transition, and emits the async CSI = 7 ; <ch> ; 0 n notification. */
void audio_apc_poll(struct cterminal *cterm);

/* Session teardown — free all patch slots, close all APC-owned channel
 * handles.  Called from the place JXL's pixmap cleanup runs. */
void audio_apc_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* _SYNCTERM_AUDIO_APC_H_ */
