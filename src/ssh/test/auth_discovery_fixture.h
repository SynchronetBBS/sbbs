#ifndef DSSH_AUTH_DISCOVERY_FIXTURE_H
#define DSSH_AUTH_DISCOVERY_FIXTURE_H

#include <stdio.h>
#include <stdlib.h>
#include "deucessh-auth.h"
#include "dssh_test_internal.h"

/* Fixture convenience only: execute the selected authentication action, then
 * independently read its cached methods. Getter failure is a fixture failure,
 * not a replacement authentication result. Native action errors leave output
 * untouched. Tests of short/sizing outputs exercise the getter directly. */
static inline int
test_auth_discover_with(int (*begin)(dssh_session, const char *),
    int64_t (*read_methods)(dssh_session, char *, size_t),
    dssh_session sess, const char *username, char *methods, size_t capacity)
{
	int result = begin(sess, username);
	if (result < 0)
		return result;
	int64_t needed = read_methods(sess, methods, capacity);
	if (needed < 0) {
		long long error = (long long)needed;
		fprintf(stderr, "auth discovery fixture could not read its snapshot: %lld\n", error);
		abort();
	}
	return result;
}

static inline int
test_auth_discover(dssh_session sess, const char *username, char *methods, size_t capacity)
{
	return test_auth_discover_with(dssh_auth_begin, dssh_auth_get_methods, sess, username, methods, capacity);
}

static inline int
test_auth_discover_component(dssh_session sess, const char *username, char *methods, size_t capacity)
{
	return test_auth_discover_with(auth_begin_impl, auth_get_methods_inner, sess, username, methods, capacity);
}

#endif
