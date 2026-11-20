/* Application-only observations of the actual example callbacks. No library
 * allocator, registry reset or private session representation is used. */
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "deucessh.h"
#include "deucessh-auth.h"

static FILE *fixture_output;
static bool reject_banner;
static uint32_t fixture_choice;
static char observed_banner[4096];
static bool fail_format;
static int observe_banner(dssh_session, const char *, const char *);
static int observe_format(char *, size_t, const char *, va_list);
#ifndef DSSH_GAME_SERVER
static uint32_t observe_uniform(uint32_t);
#endif

#define vsnprintf observe_format
#include "../examples/server-output.h"
#undef vsnprintf

#define main example_server_main
#define dssh_auth_set_banner observe_banner
#define arc4random_uniform observe_uniform
#undef stderr
#define stderr fixture_output
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#ifdef DSSH_GAME_SERVER
#include "../examples/kbi-game-server.c"
#define OUTPUT_CASES 8
#define CALLBACK_BASE 3
#define EXAMPLE_NAME "game"
#else
#include "../examples/server.c"
#define OUTPUT_CASES 12
#define CALLBACK_BASE 7
#define EXAMPLE_NAME "server"
#endif
#pragma GCC diagnostic pop
#undef stderr
#undef arc4random_uniform
#undef dssh_auth_set_banner
#undef main

#ifndef DSSH_GAME_SERVER
static uint32_t
observe_uniform(uint32_t upper)
{
	if (upper == 0)
		abort();
	return fixture_choice < upper ? fixture_choice : 0;
}
#endif

static int observe_format(char *, size_t, const char *, va_list) DSSH_EXAMPLE_PRINTF_LIKE(3, 0);

static int
observe_format(char *output, size_t capacity, const char *format, va_list arguments)
{
	if (fail_format) {
		errno = EILSEQ;
		return -1;
	}
	return vsnprintf(output, capacity, format, arguments);
}

static int
observe_banner(dssh_session session, const char *message, const char *language)
{
	size_t length = strlen(message);
	if (length >= sizeof(observed_banner))
		return DSSH_ERROR_TOOLONG;
	memcpy(observed_banner, message, length + 1);
	if (reject_banner)
		return DSSH_ERROR_ALLOC;
	return dssh_auth_set_banner(session, message, language);
}

static bool
one_output_case(unsigned which)
{
	bool valid = true;
	bool broken_log = which == OUTPUT_CASES - 1;
	fixture_output = tmpfile();
	if (fixture_output == NULL)
		return false;
	if (broken_log) {
		if (fclose(fixture_output) != 0)
			return false;
		int descriptors[2];
		if (pipe(descriptors) != 0)
			return false;
		if (close(descriptors[0]) != 0) {
			if (close(descriptors[1]) != 0)
				abort();
			return false;
		}
		fixture_output = fdopen(descriptors[1], "w");
		if (fixture_output == NULL) {
			if (close(descriptors[1]) != 0)
				abort();
			return false;
		}
		valid = setvbuf(fixture_output, NULL, _IONBF, 0) == 0;
	}
	atomic_store(&diagnostic_failed, false);
	sess = dssh_session_init(false, 0);
	valid &= sess != NULL;
	reject_banner = which == 1;
	fixture_choice = 0;
	observed_banner[0] = 0;
	const uint8_t username[] = { 'a', '\n', 27, '\\', 0, 'z', 255 };
	struct dssh_auth_challenge challenge = {0};
	struct dssh_bytes_result prompt = {0};
	int result = 0;
	if (valid && which < 2) {
		static const uint8_t method[] = "keyboard-interactive";
		valid = auth_request(username, sizeof(username), method, sizeof(method) - 1, true, NULL) == 0;
		result = auth_kbi(username, sizeof(username), 0, NULL, NULL, &challenge, NULL);
		valid &= result == (which == 1 ? DSSH_AUTH_DISCONNECT : DSSH_AUTH_KBI_PROMPT);
		if (which == 0)
			valid &= strstr(observed_banner, "a\\x0a\\x1b\\x5c\\x00z\\xff") != NULL;
	}
	else if (valid && which == 2) {
		debug_cb(true, username, sizeof(username), NULL);
		debug_cb(false, username, sizeof(username), NULL);
	}
#ifndef DSSH_GAME_SERVER
	else if (valid && which < CALLBACK_BASE) {
		static const uint8_t credential[] = "fixture-secret-value";
		static const uint8_t replacement[] = "fixture-replacement-value";
		static const uint8_t user[] = "fixture";
		if (which == 3 || which == 4 || which == 6) {
			fixture_choice = which == 4 ? 1 : 0;
			reject_banner = which == 6;
			result = auth_password(user, sizeof(user) - 1, credential, sizeof(credential) - 1, &prompt, NULL);
			valid &= result == (which == 6 ? DSSH_AUTH_DISCONNECT
			    : which == 4 ? DSSH_AUTH_CHANGE_PASSWORD : DSSH_AUTH_SUCCESS);
		}
		else {
			result = auth_passwd_change(user, sizeof(user) - 1,
			    credential, sizeof(credential) - 1, replacement, sizeof(replacement) - 1, &prompt, NULL);
			valid &= result == DSSH_AUTH_SUCCESS;
		}
		valid &= strstr(observed_banner, "fixture-secret-value") == NULL
		    && strstr(observed_banner, "fixture-replacement-value") == NULL;
	}
#endif
	else if (valid && which < OUTPUT_CASES - 1) {
		char term[] = { 't', 27, 0 };
		char name[] = { 'n', 27, 0 };
		char value[] = "fixture-secret-value";
		struct dssh_chan_env environment = { .name = name, .value = value };
		struct dssh_chan_params params = { .term = term, .cols = UINT32_MAX, .rows = 1,
		    .env = &environment, .env_count = 1, .command = value };
		struct dssh_chan_accept_result accepted = {0};
		if (which == CALLBACK_BASE) {
			result = accept_pty_cb(NULL, &params, NULL);
			unimplemented_cb(UINT32_MAX, NULL);
		}
		else if (which == CALLBACK_BASE + 1)
			result = accept_env_cb(NULL, &params, NULL);
		else if (which == CALLBACK_BASE + 2)
			result = accept_exec_cb(NULL, &params, &accepted, NULL);
		else {
			result = global_request_cb(username, sizeof(username), false, NULL, 0, NULL);
			valid &= result == -1;
			result = global_request_cb(username, sizeof(username), true, NULL, 0, NULL);
		}
		valid &= result == (which == CALLBACK_BASE + 3 ? -1 : 0);
	}
	else if (valid && broken_log) {
		static const uint8_t message[] = "fixture";
		debug_cb(true, message, sizeof(message) - 1, NULL);
		valid &= dssh_session_is_terminated(sess);
	}
	if (!broken_log) {
		char text[8192];
		valid &= fflush(fixture_output) == 0 && fseek(fixture_output, 0, SEEK_SET) == 0;
		size_t length = fread(text, 1, sizeof(text) - 1, fixture_output);
		text[length] = 0;
		valid &= ferror(fixture_output) == 0 && memchr(text, 27, length) == NULL
		    && strstr(text, "fixture-secret-value") == NULL
		    && strstr(text, "fixture-replacement-value") == NULL;
		if (which == 2)
			valid &= strstr(text, "a\\x0a\\x1b\\x5c\\x00z\\xff") != NULL;
	}
	else
		clearerr(fixture_output);
	dssh_result_release(&challenge.lease);
	dssh_result_release(&prompt.lease);
	dssh_session_cleanup(sess);
	sess = NULL;
	valid &= fclose(fixture_output) == 0;
	fixture_output = NULL;
	printf("server_output example=%s case=%u result=%d valid=%d\n", EXAMPLE_NAME, which, result, valid);
	return valid;
}

static bool
formatting_checks(void)
{
	uint8_t binary[256];
	char expected[1025];
	size_t expected_len = 0;
	for (unsigned i = 0; i < 256; i++) {
		uint8_t byte = (uint8_t)i;
		binary[i] = byte;
		if (i >= 32 && i <= 126 && i != '\\' && i != '\'' && i != '"') {
			char plain = (char)i;
			expected[expected_len++] = plain;
		}
		else {
			int encoded = snprintf(&expected[expected_len], sizeof(expected) - expected_len, "\\x%02x", i);
			if (encoded != 4)
				return false;
			expected_len += 4;
		}
	}
	expected[expected_len] = 0;
	bool valid = true;
	for (size_t capacity = 4; capacity <= 1025; capacity++) {
		char guarded[1029];
		memset(guarded, 'X', sizeof(guarded));
		guarded[sizeof(guarded) - 1] = 0;
		valid &= example_display_text(&guarded[2], capacity, binary, sizeof(binary)) == 0;
		size_t length = strlen(&guarded[2]);
		valid &= guarded[1] == 'X' && guarded[2 + capacity] == 'X' && length < capacity;
		if (expected_len < capacity)
			valid &= length == expected_len && memcmp(&guarded[2], expected, expected_len) == 0;
		else {
			valid &= length >= 3;
			if (length >= 3) {
				valid &= memcmp(&guarded[2 + length - 3], "...", 3) == 0;
				valid &= memcmp(&guarded[2], expected, length - 3) == 0;
				/* Elision must follow a complete encoded input byte. */
				size_t at = 0;
				while (at < length - 3)
					at += guarded[2 + at] == '\\' ? 4 : 1;
				valid &= at == length - 3;
			}
		}
	}
	FILE *stream = tmpfile();
	if (stream == NULL)
		return false;
	valid &= example_write_text(stream, binary, sizeof(binary)) == 0
	    && fflush(stream) == 0 && fseek(stream, 0, SEEK_SET) == 0;
	char streamed[1025];
	size_t streamed_len = fread(streamed, 1, sizeof(streamed), stream);
	valid &= ferror(stream) == 0 && streamed_len == expected_len && memcmp(streamed, expected, expected_len) == 0;
	valid &= fclose(stream) == 0;
	int descriptors[2];
	if (pipe(descriptors) != 0)
		return false;
	if (close(descriptors[0]) != 0) {
		if (close(descriptors[1]) != 0)
			abort();
		return false;
	}
	FILE *broken = fdopen(descriptors[1], "w");
	if (broken == NULL) {
		if (close(descriptors[1]) != 0)
			abort();
		return false;
	}
	valid &= setvbuf(broken, NULL, _IONBF, 0) == 0;
	valid &= example_write_text(broken, binary, 1) == DSSH_ERROR_INIT;
	clearerr(broken);
	valid &= fclose(broken) == 0;
	memset(binary, 0, sizeof(binary));
	char zeros[1025];
	for (size_t i = 0; i < sizeof(binary); i++)
		memcpy(&zeros[4 * i], "\\x00", 4);
	zeros[1024] = 0;
	char maximum[1025];
	valid &= example_display_text(maximum, sizeof(maximum), binary, sizeof(binary)) == 0
	    && strcmp(maximum, zeros) == 0;
	valid &= example_display_text(maximum, sizeof(maximum) - 1, binary, sizeof(binary)) == 0
	    && strlen(maximum) == 1023 && memcmp(maximum, zeros, 1020) == 0
	    && strcmp(&maximum[1020], "...") == 0;
	char short_text[5] = "XXXX";
	valid &= example_display_text(short_text, sizeof(short_text), NULL, 0) == 0 && short_text[0] == 0;
	valid &= example_display_text(short_text, sizeof(short_text), NULL, 1) == DSSH_ERROR_INVALID;
	static const uint8_t exact[] = "abcd";
	valid &= example_display_text(short_text, sizeof(short_text), exact, sizeof(exact) - 1) == 0
	    && strcmp(short_text, "abcd") == 0;
	valid &= example_format(short_text, sizeof(short_text), "%s", "abcd") == 0 && strcmp(short_text, "abcd") == 0;
	valid &= example_format(short_text, sizeof(short_text), "%s", "abcde") == DSSH_ERROR_TOOLONG;
	fail_format = true;
	valid &= example_format(short_text, sizeof(short_text), "%s", "a") == DSSH_ERROR_INIT;
	fail_format = false;
	valid &= example_format(short_text, 0, "%s", "a") == DSSH_ERROR_INVALID;
	printf("server_output_format octets=256 capacities=1022 boundaries=2 valid=%d\n", valid);
	return valid;
}

int
main(void)
{
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
		return 2;
	unsigned failures = 0;
	for (unsigned which = 0; which < OUTPUT_CASES; which++) {
		if (!one_output_case(which))
			failures++;
	}
	bool formatted = formatting_checks();
	if (!formatted)
		failures++;
	printf("server_output example=%s cases=%u failures=%u formatter=%d\n", EXAMPLE_NAME, OUTPUT_CASES, failures, formatted);
	return failures == 0 ? 0 : 1;
}
