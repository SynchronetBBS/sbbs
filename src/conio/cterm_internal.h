/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 ****************************************************************************/

/*
 * cterm_internal.h — shared infrastructure used by cterm.c and every
 * per-standard handler file.  Not part of the public cterm API.
 *
 * Covers:
 *   - 21-bit sequence key encoding (SEQ_* macros) + common introducers
 *   - Dispatch table entry type and parser state enum
 *   - Coordinate-system helpers (SCR_XY / CURR_XY / TERM_XY, bounds)
 *   - Screen/cursor/tab/output helpers defined in cterm.c
 *   - Shared accumulators and the master cterm_ansi_dispatch[] table
 *   - SGR parameter parsing helpers
 *   - Cascade-consumption inline helpers
 *
 * Per-emulation handler prototypes live in their own headers:
 *   cterm_ecma48.h, cterm_dec.h, cterm_cterm.h,
 *   cterm_vt52.h, cterm_atascii.h, cterm_petscii.h, cterm_prestel.h
 */

#ifndef _CTERM_INTERNAL_H_
#define _CTERM_INTERNAL_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "cterm.h"	/* enum seq_phase, struct cterminal fwd decl */

/* Maximum parsed parameter count for a CSI sequence.  Overflow beyond
 * this point causes the whole sequence to be discarded (SEQ_BROKEN).
 * ECMA-48 does not define a limit; real-world sequences rarely exceed
 * a handful of parameters. */
#define MAX_SEQ_PARAMS 64

/* ------------------------------------------------------------------------
 * Sequence key encoding (fits in 21 bits of a uint32_t)
 *
 *  bit:  20         15 14        8 7    5 4        0
 *        ┌───────────┬──────────┬──────┬──────────┐
 *        │ introducer│  final   │ priv │ intermed │
 *        │   6 bits  │  7 bits  │3 bits│  5 bits  │
 *        └───────────┴──────────┴──────┴──────────┘
 *
 *  Introducer (6 bits):  C0 0x00-0x1F  -> byte value (0-31)
 *                        C1 0x40-0x5F  -> (byte - 0x40) + 32 (32-63)
 *  Final      (7 bits):  0 = none, 1-79 = 0x30-0x7E (byte - 0x2F)
 *  Private    (3 bits):  0 = none, 1 = '<', 2 = '=', 3 = '>', 4 = '?'
 *  Intermed   (5 bits):  0 = none, 1-16 = 0x20-0x2F (byte - 0x1F)
 *
 *  Ordering the fields this way (introducer > final > private >
 *  intermediate) makes the natural uint32_t sort of the key group
 *  entries first by introducer, then by final byte, keeping related
 *  sequences adjacent in the dispatch table.
 *
 *  Only the LAST intermediate byte is recorded in the key -- multi-
 *  intermediate sequences (rare in practice) collapse onto the same key.
 * ------------------------------------------------------------------------ */

#define SEQ_INTRO_SHIFT    15
#define SEQ_FINAL_SHIFT     8
#define SEQ_PRIV_SHIFT      5
#define SEQ_INTERM_SHIFT    0

#define SEQ_INTRO_MASK   0x3Fu	/* 6 bits */
#define SEQ_PRIV_MASK    0x07u	/* 3 bits */
#define SEQ_INTERM_MASK  0x1Fu	/* 5 bits */
#define SEQ_FINAL_MASK   0x7Fu	/* 7 bits */

/* Encode raw bytes into field values.  Values are cast to uint32_t
 * before the shift so that the top 11 bits stay clear on any platform. */
#define SEQ_C0(ch)	((uint32_t)(ch) << SEQ_INTRO_SHIFT)
#define SEQ_C1(ch)	((uint32_t)((unsigned)(ch) - 0x40u + 32u) << SEQ_INTRO_SHIFT)
#define SEQ_PRIV(ch)	((uint32_t)((unsigned)(ch) - 0x3Bu) << SEQ_PRIV_SHIFT)
#define SEQ_INTERM(ch)	((uint32_t)((unsigned)(ch) - 0x1Fu) << SEQ_INTERM_SHIFT)
#define SEQ_FINAL(ch)	((uint32_t)((unsigned)(ch) - 0x2Fu) << SEQ_FINAL_SHIFT)

/* ----- common introducers --------------------------------------------- */

#define SEQ_ESC	SEQ_C0(0x1B)
#define SEQ_CSI	SEQ_C1('[')
#define SEQ_DCS	SEQ_C1('P')
#define SEQ_OSC	SEQ_C1(']')
#define SEQ_SS2	SEQ_C1('N')
#define SEQ_SS3	SEQ_C1('O')
#define SEQ_APC	SEQ_C1('_')
#define SEQ_PM	SEQ_C1('^')
#define SEQ_SOS	SEQ_C1('X')
#define SEQ_ST	SEQ_C1('\\')

/* ----- bare C0 controls ------------------------------------------------ */

#define SEQ_NUL	SEQ_C0(0x00)
#define SEQ_BEL	SEQ_C0(0x07)
#define SEQ_BS	SEQ_C0(0x08)
#define SEQ_HT	SEQ_C0(0x09)
#define SEQ_LF	SEQ_C0(0x0A)
#define SEQ_VT	SEQ_C0(0x0B)
#define SEQ_FF	SEQ_C0(0x0C)
#define SEQ_CR	SEQ_C0(0x0D)
#define SEQ_SO	SEQ_C0(0x0E)
#define SEQ_SI	SEQ_C0(0x0F)
#define SEQ_CAN	SEQ_C0(0x18)
#define SEQ_SUB	SEQ_C0(0x1A)

/* ----- C1 standalone (no final) --------------------------------------- */

#define SEQ_RI	SEQ_C1('M')
#define SEQ_NEL	SEQ_C1('E')
#define SEQ_HTS	SEQ_C1('H')
#define SEQ_STS	SEQ_C1('S')
#define SEQ_IND	SEQ_C1('D')

/* ----- ESC + Fp/Fs final (no intermediate) ---------------------------- */

#define SEQ_DECSC	(SEQ_ESC | SEQ_FINAL('7'))
#define SEQ_DECRC	(SEQ_ESC | SEQ_FINAL('8'))
#define SEQ_DECKPAM	(SEQ_ESC | SEQ_FINAL('='))
#define SEQ_DECKPNM	(SEQ_ESC | SEQ_FINAL('>'))
#define SEQ_RIS		(SEQ_ESC | SEQ_FINAL('c'))
#define SEQ_LS2		(SEQ_ESC | SEQ_FINAL('n'))
#define SEQ_LS3		(SEQ_ESC | SEQ_FINAL('o'))
#define SEQ_LS3R	(SEQ_ESC | SEQ_FINAL('|'))
#define SEQ_LS2R	(SEQ_ESC | SEQ_FINAL('}'))
#define SEQ_LS1R	(SEQ_ESC | SEQ_FINAL('~'))

/* ----- ESC + intermediate + final (ECMA-35 designation) --------------- */

#define SEQ_GZD4(f)	(SEQ_ESC | SEQ_INTERM('(') | SEQ_FINAL(f))
#define SEQ_G1D4(f)	(SEQ_ESC | SEQ_INTERM(')') | SEQ_FINAL(f))
#define SEQ_G2D4(f)	(SEQ_ESC | SEQ_INTERM('*') | SEQ_FINAL(f))
#define SEQ_G3D4(f)	(SEQ_ESC | SEQ_INTERM('+') | SEQ_FINAL(f))
#define SEQ_DOCS(f)	(SEQ_ESC | SEQ_INTERM('%') | SEQ_FINAL(f))

/* ----- CSI + final (no intermediate, no private) ---------------------- */

#define SEQ_ICH		(SEQ_CSI | SEQ_FINAL('@'))
#define SEQ_CUU		(SEQ_CSI | SEQ_FINAL('A'))
#define SEQ_CUD		(SEQ_CSI | SEQ_FINAL('B'))
#define SEQ_CUF		(SEQ_CSI | SEQ_FINAL('C'))
#define SEQ_CUB		(SEQ_CSI | SEQ_FINAL('D'))
#define SEQ_CNL		(SEQ_CSI | SEQ_FINAL('E'))
#define SEQ_CPL		(SEQ_CSI | SEQ_FINAL('F'))
#define SEQ_CHA		(SEQ_CSI | SEQ_FINAL('G'))
#define SEQ_CUP		(SEQ_CSI | SEQ_FINAL('H'))
#define SEQ_CHT		(SEQ_CSI | SEQ_FINAL('I'))
#define SEQ_ED		(SEQ_CSI | SEQ_FINAL('J'))
#define SEQ_EL		(SEQ_CSI | SEQ_FINAL('K'))
#define SEQ_IL		(SEQ_CSI | SEQ_FINAL('L'))
#define SEQ_DL		(SEQ_CSI | SEQ_FINAL('M'))
#define SEQ_DCH		(SEQ_CSI | SEQ_FINAL('P'))
#define SEQ_SEE		(SEQ_CSI | SEQ_FINAL('Q'))
#define SEQ_CPR		(SEQ_CSI | SEQ_FINAL('R'))
#define SEQ_SU		(SEQ_CSI | SEQ_FINAL('S'))
#define SEQ_SD		(SEQ_CSI | SEQ_FINAL('T'))
#define SEQ_NP		(SEQ_CSI | SEQ_FINAL('U'))
#define SEQ_PP		(SEQ_CSI | SEQ_FINAL('V'))
#define SEQ_CTC		(SEQ_CSI | SEQ_FINAL('W'))
#define SEQ_ECH		(SEQ_CSI | SEQ_FINAL('X'))
#define SEQ_CVT		(SEQ_CSI | SEQ_FINAL('Y'))
#define SEQ_CBT		(SEQ_CSI | SEQ_FINAL('Z'))
#define SEQ_SRS		(SEQ_CSI | SEQ_FINAL('['))
#define SEQ_PTX		(SEQ_CSI | SEQ_FINAL('\\'))
#define SEQ_SDS		(SEQ_CSI | SEQ_FINAL(']'))
#define SEQ_SIMD	(SEQ_CSI | SEQ_FINAL('^'))
#define SEQ_HPA		(SEQ_CSI | SEQ_FINAL('`'))
#define SEQ_HPR		(SEQ_CSI | SEQ_FINAL('a'))
#define SEQ_REP		(SEQ_CSI | SEQ_FINAL('b'))
#define SEQ_DA		(SEQ_CSI | SEQ_FINAL('c'))
#define SEQ_VPA		(SEQ_CSI | SEQ_FINAL('d'))
#define SEQ_VPR		(SEQ_CSI | SEQ_FINAL('e'))
#define SEQ_HVP		(SEQ_CSI | SEQ_FINAL('f'))
#define SEQ_TBC		(SEQ_CSI | SEQ_FINAL('g'))
#define SEQ_SM		(SEQ_CSI | SEQ_FINAL('h'))
#define SEQ_MC		(SEQ_CSI | SEQ_FINAL('i'))
#define SEQ_HPB		(SEQ_CSI | SEQ_FINAL('j'))
#define SEQ_VPB		(SEQ_CSI | SEQ_FINAL('k'))
#define SEQ_RM		(SEQ_CSI | SEQ_FINAL('l'))
#define SEQ_SGR		(SEQ_CSI | SEQ_FINAL('m'))
#define SEQ_DSR		(SEQ_CSI | SEQ_FINAL('n'))
#define SEQ_DAQ		(SEQ_CSI | SEQ_FINAL('o'))
#define SEQ_DECSTBM	(SEQ_CSI | SEQ_FINAL('r'))
#define SEQ_SCOSC	(SEQ_CSI | SEQ_FINAL('s'))
#define SEQ_SCORC	(SEQ_CSI | SEQ_FINAL('u'))

/* ----- CSI + private + final (DEC private) ---------------------------- */

#define SEQ_DECSED	(SEQ_CSI | SEQ_PRIV('?') | SEQ_FINAL('J'))
#define SEQ_DECSEL	(SEQ_CSI | SEQ_PRIV('?') | SEQ_FINAL('K'))
#define SEQ_DECXCPR	(SEQ_CSI | SEQ_PRIV('?') | SEQ_FINAL('R'))
#define SEQ_DECSM	(SEQ_CSI | SEQ_PRIV('?') | SEQ_FINAL('h'))
#define SEQ_DECRM	(SEQ_CSI | SEQ_PRIV('?') | SEQ_FINAL('l'))
#define SEQ_DECDSR	(SEQ_CSI | SEQ_PRIV('?') | SEQ_FINAL('n'))
#define SEQ_DECRQM	(SEQ_CSI | SEQ_PRIV('?') | SEQ_FINAL('p'))

/* ----- CSI + '>' private (xterm secondary DA / modkeys) --------------- */

#define SEQ_XTDA	(SEQ_CSI | SEQ_PRIV('>') | SEQ_FINAL('c'))
#define SEQ_XTMODKEYS	(SEQ_CSI | SEQ_PRIV('>') | SEQ_FINAL('m'))

/* ----- CSI = (SyncTERM private) --------------------------------------- */

#define SEQ_STDA	(SEQ_CSI | SEQ_PRIV('=') | SEQ_FINAL('c'))
#define SEQ_STMUSIC	(SEQ_CSI | SEQ_PRIV('=') | SEQ_FINAL('M'))
#define SEQ_STGFX	(SEQ_CSI | SEQ_PRIV('=') | SEQ_FINAL('S'))

/* ----- CSI + intermediate + final ------------------------------------- */

#define SEQ_DECSCUSR	(SEQ_CSI | SEQ_INTERM(' ') | SEQ_FINAL('q'))
#define SEQ_DECCRA	(SEQ_CSI | SEQ_INTERM('$') | SEQ_FINAL('v'))
#define SEQ_DECRQSS	(SEQ_CSI | SEQ_INTERM('$') | SEQ_FINAL('p'))
#define SEQ_DECFRA	(SEQ_CSI | SEQ_INTERM('$') | SEQ_FINAL('x'))
#define SEQ_DECERA	(SEQ_CSI | SEQ_INTERM('$') | SEQ_FINAL('z'))
#define SEQ_DECTABSR	(SEQ_CSI | SEQ_INTERM('$') | SEQ_FINAL('w'))
#define SEQ_DECRARA	(SEQ_CSI | SEQ_INTERM('$') | SEQ_FINAL('t'))
#define SEQ_DECCARA	(SEQ_CSI | SEQ_INTERM('$') | SEQ_FINAL('r'))
#define SEQ_DECSCS	(SEQ_CSI | SEQ_INTERM('*') | SEQ_FINAL('r'))
#define SEQ_DECSACE	(SEQ_CSI | SEQ_INTERM('*') | SEQ_FINAL('x'))
#define SEQ_DECMACRO	(SEQ_CSI | SEQ_INTERM('*') | SEQ_FINAL('z'))
#define SEQ_DECIC	(SEQ_CSI | SEQ_INTERM('\'') | SEQ_FINAL('}'))
#define SEQ_DECDC	(SEQ_CSI | SEQ_INTERM('\'') | SEQ_FINAL('~'))

/* ------------------------------------------------------------------------
 * Parser / dispatch types
 * ------------------------------------------------------------------------ */

enum sequence_state {
	SEQ_BROKEN,
	SEQ_INCOMPLETE,
	SEQ_COMPLETE
};

/* One dispatch entry.  Tables are sorted by key and searched via bsearch(). */
struct seq_dispatch {
	void		(*handler)(struct cterminal *cterm, int *speed);
	uint32_t	key;
	uint8_t		trailing;	/* raw data bytes to accumulate after key match */
};

#ifndef SEQ_ARRAY_SIZE
#define SEQ_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

/* ------------------------------------------------------------------------
 * Coordinate system helpers shared by all handler files
 * ------------------------------------------------------------------------ */

enum cterm_coordinates {
	CTERM_COORD_SCREEN,
	CTERM_COORD_ABSTERM,
	CTERM_COORD_TERM,
	CTERM_COORD_CURR,
};

/* Min/max bounds of the three coordinate spaces.  All are zero-arg macros
 * that expand to expressions reading `cterm`. */
#define SCR_MINX	1
#define ABS_MINX	1
#define TERM_MINX	1
#define CURR_MINX	1
#define ABS_MAXX	cterm->width
#define TERM_MAXX	(cterm->right_margin - cterm->left_margin + 1)
#define CURR_MAXX	((cterm->extattr & CTERM_EXTATTR_ORIGINMODE) ? TERM_MAXX : ABS_MAXX)
#define SCR_MINY	1
#define ABS_MINY	1
#define TERM_MINY	1
#define CURR_MINY	1
#define ABS_MAXY	cterm->height
#define TERM_MAXY	(cterm->bottom_margin - cterm->top_margin + 1)
#define CURR_MAXY	((cterm->extattr & CTERM_EXTATTR_ORIGINMODE) ? TERM_MAXY : ABS_MAXY)

#define SCR_XY(x,y)	cterm_coord_get_xy(cterm, CTERM_COORD_SCREEN, x, y)
#define ABS_XY(x,y)	cterm_coord_get_xy(cterm, CTERM_COORD_ABSTERM, x, y)
#define TERM_XY(x,y)	cterm_coord_get_xy(cterm, CTERM_COORD_TERM, x, y)
#define CURR_XY(x,y)	cterm_coord_get_xy(cterm, CTERM_COORD_CURR, x, y)

void cterm_coord_conv_xy(struct cterminal *cterm,
    enum cterm_coordinates from_coord, enum cterm_coordinates to_coord,
    int *x, int *y);
void cterm_coord_get_xy(struct cterminal *cterm,
    enum cterm_coordinates coord, int *x, int *y);

/* ------------------------------------------------------------------------
 * Screen / cursor helpers
 * ------------------------------------------------------------------------ */

void cterm_clear_lcf(struct cterminal *cterm);
void cterm_adjust_currpos(struct cterminal *cterm, int xadj, int yadj, int scroll);
void cterm_gotoxy(struct cterminal *cterm, int x, int y);
void cterm_scrollup(struct cterminal *cterm);
void cterm_scrolldown(struct cterminal *cterm);
void cterm_clreol(struct cterminal *cterm);
void cterm_clear2bol(struct cterminal *cterm);
void cterm_dellines(struct cterminal *cterm, int lines);
void cterm_clear_selection(struct cterminal *cterm);
void cterm_fill_rect(struct cterminal *cterm, int left, int top, int right,
    int bottom, uint8_t ch);
void cterm_transmit_selected(struct cterminal *cterm);

/* Tab stops */
void cterm_do_tab(struct cterminal *cterm);
void cterm_do_vtab(struct cterminal *cterm);
void cterm_do_backtab(struct cterminal *cterm);
void cterm_insert_tabstop(struct cterminal *cterm, int pos);
void cterm_delete_tabstop(struct cterminal *cterm, int pos);
void cterm_insert_vtabstop(struct cterminal *cterm, int line);
void cterm_delete_vtabstop(struct cterminal *cterm, int line);

/* Output */
void cterm_ctputs(struct cterminal *cterm, char *buf);
void cterm_uctputs(struct cterminal *cterm, char *buf);
void cterm_respond(struct cterminal *cterm, const char *data, size_t len);
void cterm_respond_printf(struct cterminal *cterm, const char *fmt, ...);
int cterm_setpalette(struct cterminal *cterm, uint32_t entry,
    uint16_t r, uint16_t g, uint16_t b);

void cterm_reset(struct cterminal *cterm);
void cterm_set_negative(struct cterminal *cterm, bool on);
void cterm_cond_scrollup(struct cterminal *cterm);
void cterm_advance_char(struct cterminal *cterm, int *x, int *y, int move);

/* Uchar wrapper for cterm_ctputs — the print-batch buffer holds
 * unsigned chars but cterm_ctputs takes a signed char*. */
#define cterm_uctputs(c, p)	cterm_ctputs(c, (char *)p)
#define ustrcat(b, s)		strcat((char *)(b), (const char *)(s))
#define ustrlen(s)		strlen((const char *)(s))

/* Signature shared by every byte-accumulator installed in
 * cterm->accumulator.  Returns true when the byte was consumed (stay
 * in the accumulator next iteration too), false to hand the byte off
 * to cterm->dispatch after all. */
typedef bool (*cterm_accumulator_fn)(struct cterminal *, unsigned char, int *);

/* Generic "accumulate N raw bytes, then call the stored handler"
 * accumulator, in cterm.c.  Installed by dispatch entries whose
 * `trailing` field is non-zero (ANSI sequence table + VT52 ESC Y/b/c)
 * and by BEEB VDU 23 / 28 which wire it up directly. */
bool cterm_accumulate_trailing(struct cterminal *cterm, unsigned char byte,
    int *speed);

/* Shared accumulators and dispatchers in cterm.c.  The ECMA-48 sequence
 * accumulator and dispatcher are shared between ANSI-BBS and Atari ST
 * VT52 (VT52 installs its own single-byte dispatcher, declared in
 * cterm_vt52.h, but routes sequence parsing through the same path).
 * cterm_dispatch_byte drives ATASCII and PETSCII via their bitmap +
 * handler tables. */
bool cterm_accumulate_ecma_seq(struct cterminal *cterm, unsigned char byte,
    int *speed);
void cterm_dispatch_ecma(struct cterminal *cterm, unsigned char byte,
    int *speed);
void cterm_dispatch_byte(struct cterminal *cterm, unsigned char byte,
    int *speed);

/* Master ANSI-BBS dispatch table defined in cterm.c.  References handlers
 * from all three per-standard files (cterm_ecma48.c, cterm_dec.c,
 * cterm_cterm.c). */
extern const struct seq_dispatch cterm_ansi_dispatch[];
extern const size_t cterm_ansi_dispatch_len;

/* SkyPix palette translation — a no-op when cterm->skypix is clear,
 * otherwise remaps ANSI colour indices to SkyPix palette positions.
 * Called inline from the ECMA-48 SGR handler (cases 30-37 / 40-47). */
int cterm_skypix_color(struct cterminal *cterm, int color);

/* Fg / bg attribute setters defined in cterm.c.  Honour cterm->negative
 * (swap fg/bg) and update cterm->fg_color / cterm->bg_color via
 * attr2palette.  Used by Prestel, the SGR colour handlers, and anyone
 * else that needs to paint a palette-index colour onto the current
 * attribute byte. */
void cterm_set_fgattr(struct cterminal *cterm, unsigned char colour);
void cterm_set_bgattr(struct cterminal *cterm, unsigned char colour);


/* ------------------------------------------------------------------------
 * SGR helpers
 *
 * cterm_apply_sgr is called once per SGR parameter from cterm_handle_sgr
 * and advances *pi past any consumed sub-arguments (e.g. SGR 38;2;R;G;B
 * consumes five indices).  cterm_parse_extended_colour implements the
 * 38/48 sub-parameter parsing for indexed/RGB colour.  Sub-param helpers
 * split a single colon-separated parameter (CSI 38 : 2 : R : G : B m).
 * ------------------------------------------------------------------------ */

struct sub_params {
	int		param_count;
	uint64_t	*param_int;
};

void cterm_apply_sgr(struct cterminal *cterm, int *pi);
void cterm_parse_extended_colour(struct cterminal *cterm, int *i, bool fg);
bool cterm_parse_sub_parameters(struct sub_params *sub, struct cterminal *cterm,
    int param);
void cterm_save_extended_colour_seq(struct cterminal *cterm, int fg,
    int seqoff, int count);
void cterm_free_sub_parameters(struct sub_params *sub);

/* ------------------------------------------------------------------------
 * Dispatch bridge helpers
 * ------------------------------------------------------------------------ */

int cterm_seq_dispatch_cmp(const void *key, const void *entry);
void cterm_seq_default(struct cterminal *cterm, int index, uint64_t val);

/* ------------------------------------------------------------------------
 * Cascade-consumption helpers
 *
 * When a control sequence is handled by more than one file (e.g. SGR —
 * SyncTERM extensions first, then ECMA-48 base; DECSM — SyncTERM private
 * modes first, then DEC), the upstream handler marks the parameters it
 * consumed and then calls the next handler directly.  Downstream handlers
 * iterate seq_param_int[] and skip indices where seq_consumed[] is true.
 *
 * cterm_seq_defaults_apply() returns true only when the sequence arrived
 * with zero parameters *and* nothing has been consumed — so the "empty
 * input => SGR 0" style default path cannot be triggered by an upstream
 * consumer emptying the list.
 * ------------------------------------------------------------------------ */

static inline void
cterm_seq_consume_param(struct cterminal *cterm, int i)
{
	if (i >= 0 && i < MAX_SEQ_PARAMS) {
		cterm->seq_consumed[i] = true;
		cterm->seq_consumed_any = true;
	}
}

static inline bool
cterm_seq_param_consumed(const struct cterminal *cterm, int i)
{
	if (i < 0 || i >= MAX_SEQ_PARAMS)
		return false;
	return cterm->seq_consumed[i];
}

static inline bool
cterm_seq_defaults_apply(const struct cterminal *cterm)
{
	return cterm->seq_param_count == 0 && !cterm->seq_consumed_any;
}

typedef void (*cterm_handler_fn)(struct cterminal *cterm, int *speed);

#endif /* _CTERM_INTERNAL_H_ */
