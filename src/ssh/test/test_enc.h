/*
 * test_enc.h — Test encryption module with failure injection.
 *
 * Registers as "aes256-ctr" with a simple XOR cipher.
 * Use the fail_*_at functions to make the Nth call return an error.
 * Pass -1 or call test_enc_reset() to disable injection.
 */

#ifndef DSSH_TEST_ENC_H
#define DSSH_TEST_ENC_H

/* Register the test enc module.  Call INSTEAD of register_aes256_ctr(). */
int register_test_enc(void);

/* Reset all failure injection state */
void test_enc_reset(void);

/* Fail the Nth init/encrypt/decrypt call (0-based) */
void test_enc_fail_init_at(int n);
void test_enc_fail_encrypt_at(int n);
void test_enc_fail_decrypt_at(int n);

#endif
