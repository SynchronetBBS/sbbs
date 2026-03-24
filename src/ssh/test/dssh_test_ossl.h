/*
 * dssh_test_ossl.h — OpenSSL + C11 threads failure injection.
 *
 * Under DSSH_TESTING, library calls to OpenSSL and C11 thread
 * functions are redirected via macros in ssh-internal.h.  This
 * header declares the wrapper functions and control API.
 *
 * The wrappers use a shared atomic countdown: each failable call
 * decrements the counter, and when it reaches zero, returns the
 * appropriate failure value (NULL, 0, thrd_error).  Non-failable
 * calls (free, destroy, cleanse) are not wrapped.
 */

#ifndef DSSH_TEST_OSSL_H
#define DSSH_TEST_OSSL_H

#include <stdatomic.h>

/*
 * Control API — call from test code (NOT redirected).
 */
void dssh_test_ossl_reset(void);
void dssh_test_ossl_fail_after(int n);
void dssh_test_ossl_exclude_thread(void);
int  dssh_test_ossl_count(void);

#endif /* DSSH_TEST_OSSL_H */
