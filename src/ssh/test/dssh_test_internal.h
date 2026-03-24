/*
 * dssh_test_internal.h — Declarations for functions exposed via
 * DSSH_TESTABLE when compiled with -DDSSH_TESTING.
 *
 * Only include this from test code, never from library code.
 */

#ifndef DSSH_TEST_INTERNAL_H
#define DSSH_TEST_INTERNAL_H

#include "deucessh.h"
#include "ssh-trans.h"

struct dssh_channel_s;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Reset the global algorithm registry and configuration.
 * Frees all registered algorithm entries, zeros the global config,
 * and unlocks registration for the next test group.
 *
 * Only available when the library is compiled with -DDSSH_TESTING.
 */
void dssh_test_reset_global_config(void);

/*
 * Key derivation (RFC 4253 s7.2).
 * Exposed from ssh-trans.c via DSSH_TESTABLE.
 */
int dssh_test_derive_key(const char *hash_name,
    const uint8_t *shared_secret, size_t shared_secret_sz,
    const uint8_t *hash, size_t hash_sz,
    uint8_t letter,
    const uint8_t *session_id, size_t session_id_sz,
    uint8_t *out, size_t needed);

/*
 * Algorithm negotiation: returns the first mutually-supported algorithm
 * from client_list (preference order) that also appears in server_list
 * and is registered in the linked list starting at head.
 * name_offset is the byte offset of the name[] flex member in the struct.
 * Returns node pointer or NULL.
 */
void *dssh_test_negotiate_algo(const char *client_list,
    const char *server_list, void *head, size_t name_offset);

/*
 * Build a comma-separated name-list from a linked list of algorithm
 * entries.  head is the first node, name_offset is the offset of
 * the name[] flex member.  Returns the length written.
 */
size_t dssh_test_build_namelist(void *head, size_t name_offset,
    char *buf, size_t bufsz);

/*
 * Global config — DSSH_TESTABLE in ssh-trans.c.
 */
extern struct dssh_transport_global_config gconf;

/*
 * ssh-conn.c internal functions exposed for testing.
 */
int dssh_conn_send_data(dssh_session sess, struct dssh_channel_s *ch,
    const uint8_t *data, size_t len);
int dssh_conn_send_extended_data(dssh_session sess, struct dssh_channel_s *ch,
    uint32_t data_type_code, const uint8_t *data, size_t len);
int dssh_conn_send_eof(dssh_session sess, struct dssh_channel_s *ch);
int dssh_conn_close(dssh_session sess, struct dssh_channel_s *ch);
int dssh_conn_send_window_adjust(dssh_session sess,
    struct dssh_channel_s *ch, uint32_t bytes);
void maybe_replenish_window(dssh_session sess, struct dssh_channel_s *ch);
void demux_dispatch(dssh_session sess, uint8_t msg_type,
    uint8_t *payload, size_t payload_len);

/*
 * ssh-trans.c internal functions exposed for testing.
 */
size_t first_name(const char *list, char *buf, size_t bufsz);

/*
 * Block size helpers from ssh-trans.c (BPP minimum clamping).
 */
size_t tx_block_size(dssh_session sess);
size_t rx_block_size(dssh_session sess);

/*
 * Version string validators from ssh-trans.c.
 */
bool dssh_test_has_nulls(uint8_t *buf, size_t buflen);
bool dssh_test_missing_crlf(uint8_t *buf, size_t buflen);
bool dssh_test_is_version_line(uint8_t *buf, size_t buflen);
bool dssh_test_has_non_ascii(uint8_t *buf, size_t buflen);
bool dssh_test_is_20(uint8_t *buf, size_t buflen);

/*
 * version_tx from ssh-trans.c — sends the SSH version string.
 */
int version_tx(dssh_session sess);

/*
 * DH-GEX helpers from kex/dh-gex-sha256.c.
 */
#include <openssl/bn.h>

int64_t parse_bn_mpint(const uint8_t *buf, size_t bufsz, BIGNUM **bn);
int serialize_bn_mpint(const BIGNUM *bn, uint8_t *buf, size_t bufsz, size_t *pos);
bool dh_value_valid(const BIGNUM *val, const BIGNUM *p);

#ifdef __cplusplus
}
#endif

#endif
