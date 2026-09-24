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

	printf("%d tests run, %d failed\n", tests_run, tests_failed);
	return tests_failed == 0 ? 0 : 1;
}
