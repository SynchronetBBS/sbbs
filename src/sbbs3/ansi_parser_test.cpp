/* Standalone unit tests for ansi_parser.cpp
 *
 * Build and run:
 *   g++ -Wall -Wextra -o /tmp/ansi_parser_test src/sbbs3/ansi_parser_test.cpp
 *   /tmp/ansi_parser_test
 *
 * The implementation is pulled in as a translation-unit include, the same way
 * boolsrch_test.c and termaudio_test.c do it, so this builds as a single source
 * file with no link against the rest of the tree. Do NOT also pass
 * ansi_parser.cpp on the command line, and do NOT add this file to objects.mk
 * or the MSVC projects.
 */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 ****************************************************************************/

#include <stdio.h>
#include <string>

#include "ansi_parser.cpp"

static int tests_run = 0;
static int tests_failed = 0;

#define CHECK(cond, fmt, ...)                                                 \
		do {                                                                      \
			tests_run++;                                                          \
			if (!(cond)) {                                                        \
				tests_failed++;                                                   \
				fprintf(stderr, "FAIL: " fmt "\n", ## __VA_ARGS__);               \
			}                                                                     \
		} while (0)

/* Feed a whole string through the parser, returning the final state. */
static enum ansiState feed(ANSI_Parser& p, const std::string& s)
{
	enum ansiState st = ansiState_none;
	for (size_t i = 0; i < s.length(); i++)
		st = p.parse((unsigned char)s[i]);
	return st;
}

int main(void)
{
	/* --- an ordinary CSI sequence is unaffected by the cap --- */
	{
		ANSI_Parser    p;
		enum ansiState st = feed(p, "\x1b[1;31m");
		CHECK(st == ansiState_final, "CSI reaches final state");
		CHECK(p.ansi_final_byte == 'm', "CSI final byte");
		CHECK(p.ansi_sequence == "\x1b[1;31m", "CSI sequence recorded whole");
		CHECK(p.sequence_overflow == 0, "CSI does not overflow");
	}

	/* --- a short control string is recorded whole --- */
	{
		ANSI_Parser    p;
		enum ansiState st = feed(p, "\x1b_SyncTERM:Q;libsndfile\x1b\\");
		CHECK(st == ansiState_final, "short APC reaches final state");
		CHECK(p.ansi_was_string, "short APC flagged as a string");
		CHECK(p.sequence_overflow == 0, "short APC does not overflow");
		CHECK(p.ansi_sequence.length() < ANSI_Parser::max_sequence_len,
		      "short APC stays under the cap");
	}

	/* --- a long control string is capped, and the state machine still works ---
	   This is the property that matters: an audio or graphics payload is
	   megabyte-scale base64, and only the broken-sequence warning ever reads
	   ansi_sequence. Capping must not cost us the terminating ST. */
	{
		ANSI_Parser    p;
		std::string    big = "\x1b_SyncTERM:C;S;name;";
		big.append(64 * 1024, 'A');          /* 64KB of base64-ish payload */
		big += "\x1b\\";
		enum ansiState st = feed(p, big);

		CHECK(st == ansiState_final, "long APC still reaches final state");
		CHECK(p.ansi_was_string, "long APC flagged as a string");
		CHECK(p.ansi_sequence.length() <= ANSI_Parser::max_sequence_len,
		      "long APC capped at %u, got %u"
		      , (unsigned)ANSI_Parser::max_sequence_len
		      , (unsigned)p.ansi_sequence.length());
		CHECK(p.sequence_overflow > 0, "long APC counted the discarded bytes");
		/* Every payload byte is either kept or counted, none silently lost. */
		CHECK(p.ansi_sequence.length() + p.sequence_overflow == big.length(),
		      "kept + overflow == total fed (%u + %u vs %u)"
		      , (unsigned)p.ansi_sequence.length()
		      , (unsigned)p.sequence_overflow, (unsigned)big.length());
	}

	/* --- reset() clears the overflow counter --- */
	{
		ANSI_Parser p;
		std::string big = "\x1b_x";
		big.append(4096, 'B');
		big += "\x1b\\";
		feed(p, big);
		CHECK(p.sequence_overflow > 0, "overflow set before reset");
		p.reset();
		CHECK(p.sequence_overflow == 0, "reset clears overflow");
		CHECK(p.ansi_sequence.empty(), "reset clears the sequence");
		CHECK(p.current_state() == ansiState_none, "reset clears the state");
	}

	/* --- a capped string that then goes broken is still detected --- */
	{
		ANSI_Parser    p;
		std::string    big = "\x1b_";
		big.append(4096, 'C');
		big += "\x01";                       /* illegal inside a control string */
		enum ansiState st = feed(p, big);
		CHECK(st == ansiState_broken, "illegal byte still breaks a capped string");
	}

	/* --- a caller must reconstruct a control string from its own source buffer,
	   not from ansi_sequence. putmsg() passes sequences through to the client
	   this way; using the capped copy would drop the terminator and leave the
	   client swallowing everything after it. --- */
	{
		ANSI_Parser p;
		std::string seq = "\x1b_SyncTERM:C;S;n;";
		seq.append(8192, 'D');
		seq += "\x1b\\";
		std::string tail = "AFTER";
		std::string input = seq + tail;
		std::string emitted, passthrough;
		size_t start = 0;

		for (size_t i = 0; i < input.length(); i++) {
			if (p.current_state() == ansiState_none)
				start = i;
			enum ansiState st = p.parse((unsigned char)input[i]);
			if (st == ansiState_final) {
				emitted.assign(input, start, i - start + 1);
				p.reset();
			} else if (st == ansiState_none) {
				passthrough += input[i];
			}
		}
		CHECK(emitted == seq, "source span reconstructs the whole sequence (%u of %u bytes)"
		      , (unsigned)emitted.length(), (unsigned)seq.length());
		CHECK(passthrough == tail, "text after the sequence still passes through");
		/* The capped copy is deliberately NOT sufficient for that job: */
		CHECK(p.ansi_sequence.length() < seq.length(),
		      "ansi_sequence alone would have been short");
	}

	/* --- the parser states term_out() relies on to keep a whole control string,
	   introducer and terminator included, out of the line buffer. It records
	   where a sequence begins while the parser is idle, and rolls the buffer
	   back when a sequence ends having been a string. --- */
	{
		ANSI_Parser p;
		const std::string apc = "\x1b_SyncTERM:A;Flush;C=2\x1b\\";
		enum ansiState st = ansiState_none;
		bool idle_at_esc = false, esc_before_intro = false, was_str_before_intro = true;
		bool string_before_st_esc = false, esc_before_st = false, was_str_before_st = false;

		for (size_t i = 0; i < apc.length(); i++) {
			if (i == 0)
				idle_at_esc = (p.current_state() == ansiState_none);
			if (i == 1) {
				esc_before_intro = (p.current_state() == ansiState_esc);
				was_str_before_intro = p.ansi_was_string;
			}
			if (i == apc.length() - 2)
				string_before_st_esc = (p.current_state() == ansiState_string);
			if (i == apc.length() - 1) {
				esc_before_st = (p.current_state() == ansiState_esc);
				was_str_before_st = p.ansi_was_string;
			}
			st = p.parse((unsigned char)apc[i]);
		}
		CHECK(idle_at_esc, "parser idle when the opening ESC arrives");
		CHECK(esc_before_intro && !was_str_before_intro,
		      "introducer arrives in esc state, not yet known to be a string");
		CHECK(string_before_st_esc, "terminator's ESC arrives in string state");
		CHECK(esc_before_st && was_str_before_st,
		      "terminator's final byte arrives in esc state, known to be a string");
		CHECK(st == ansiState_final && p.ansi_was_string,
		      "a completed control string reports ansi_was_string at final");
	}
	{
		ANSI_Parser p;
		enum ansiState st = feed(p, "\x1b[1;31m");
		CHECK(st == ansiState_final && !p.ansi_was_string,
		      "a completed CSI does not report ansi_was_string, so it stays in lbuf");
	}

	/* --- SOS ends on its ST, the same way the other control strings do --- */
	{
		ANSI_Parser    p;
		ANSI_Parser    apc;
		enum ansiState st = feed(p, "\x1bXhello\x1b\\");
		feed(apc, "\x1b_hello\x1b\\");
		CHECK(st == ansiState_final, "SOS reaches final state on ST (state %d)", st);
		CHECK(p.ansi_was_string, "SOS flagged as a string");
		CHECK(p.ansi_final_byte == apc.ansi_final_byte && p.ansi_was_cc == apc.ansi_was_cc,
		      "SOS ends like APC (final byte %02X, was_cc %d)", p.ansi_final_byte, p.ansi_was_cc);
		CHECK(p.ansi_sequence == "\x1bXhello\x1b\\", "SOS sequence recorded whole");
	}
	{
		ANSI_Parser    p;
		enum ansiState st = feed(p, "\x1bXa\x1b" "bc\x1b\\");
		CHECK(st == ansiState_final, "ESC + other byte inside SOS is content (state %d)", st);
	}
	{
		ANSI_Parser    p;
		enum ansiState st = feed(p, "\x1bXa\x1bX");
		CHECK(st == ansiState_broken, "SOS inside SOS is broken (state %d)", st);
	}

	/* --- CSI parameters are capped too: SGR handling rescans them once per
	   parameter, so an uncapped parameter string costs cubic time --- */
	{
		ANSI_Parser    p;
		std::string    params = "38;5;196";
		while (params.length() < 3 * ANSI_Parser::max_sequence_len)
			params += ";0";
		enum ansiState st = feed(p, "\x1b[" + params + "m");
		CHECK(st == ansiState_final, "CSI with overlong parameters reaches final state");
		CHECK(p.ansi_final_byte == 'm', "overlong CSI keeps its final byte");
		CHECK(p.ansi_params.length() <= ANSI_Parser::max_sequence_len,
		      "ansi_params capped (%u > %u)", (unsigned)p.ansi_params.length()
		      , (unsigned)ANSI_Parser::max_sequence_len);
		CHECK(p.count_params() <= ANSI_Parser::max_sequence_len,
		      "parameter count bounded by the cap (%u)", p.count_params());
		CHECK(p.get_pval(0, 0) == 38 && p.get_pval(1, 0) == 5 && p.get_pval(2, 0) == 196,
		      "leading parameters survive the cap");
	}
	{
		ANSI_Parser    p;
		enum ansiState st = feed(p, "\x1b[?" + std::string(2 * ANSI_Parser::max_sequence_len, '1') + "S");
		CHECK(st == ansiState_final && p.ansi_was_private && p.ansi_params[0] == '?',
		      "overlong private CSI keeps its private marker");
	}
	{
		ANSI_Parser    p;
		enum ansiState st = feed(p, "\x1b[" + std::string(2 * ANSI_Parser::max_sequence_len, '$') + "p");
		CHECK(st == ansiState_final && p.ansi_final_byte == 'p', "CSI with overlong intermediates reaches final");
		CHECK(p.ansi_ibs.length() <= ANSI_Parser::max_sequence_len,
		      "ansi_ibs capped (%u > %u)", (unsigned)p.ansi_ibs.length()
		      , (unsigned)ANSI_Parser::max_sequence_len);
		CHECK(p.ansi_ibs != "$", "overlong intermediates don't collapse to a single one");
	}
	{
		ANSI_Parser    p;
		enum ansiState st = feed(p, "\x1b" + std::string(2 * ANSI_Parser::max_sequence_len, '(') + "B");
		CHECK(st == ansiState_final && p.ansi_ibs.length() <= ANSI_Parser::max_sequence_len,
		      "ESC with overlong intermediates: ansi_ibs capped (%u)", (unsigned)p.ansi_ibs.length());
	}

	printf("%d tests run, %d failed\n", tests_run, tests_failed);
	return tests_failed == 0 ? 0 : 1;
}
