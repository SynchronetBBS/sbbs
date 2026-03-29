/*
 * dssh_test_internal.h -- Declarations for functions exposed via
 * DSSH_TESTABLE when compiled with -DDSSH_TESTING.
 *
 * Only include this from test code, never from library code.
 */

#ifndef DSSH_TEST_INTERNAL_H
#define DSSH_TEST_INTERNAL_H

#include "deucessh.h"
#include "ssh-internal.h"

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
int derive_key(const char *hash_name,
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
void *negotiate_algo(const char *client_list,
    const char *server_list, void *head, size_t name_offset);

/*
 * Build a comma-separated name-list from a linked list of algorithm
 * entries.  head is the first node, name_offset is the offset of
 * the name[] flex member.  Returns the length written.
 */
size_t build_namelist(void *head, size_t name_offset,
    char *buf, size_t bufsz);

/*
 * Global config -- DSSH_TESTABLE in ssh-trans.c.
 */
extern struct dssh_transport_global_config gconf;

/*
 * Channel buffer primitives from ssh-conn.c (DSSH_TESTABLE).
 */
int bytebuf_init(struct dssh_bytebuf *b, size_t capacity);
void bytebuf_free(struct dssh_bytebuf *b);
size_t bytebuf_write(struct dssh_bytebuf *b,
    const uint8_t *data, size_t len);
size_t bytebuf_read(struct dssh_bytebuf *b,
    uint8_t *buf, size_t bufsz, size_t limit);
size_t bytebuf_available(const struct dssh_bytebuf *b);
size_t bytebuf_free_space(const struct dssh_bytebuf *b);
void msgqueue_init(struct dssh_msgqueue *q);
void msgqueue_free(struct dssh_msgqueue *q);
int msgqueue_push(struct dssh_msgqueue *q,
    const uint8_t *data, size_t len);
int64_t msgqueue_pop(struct dssh_msgqueue *q,
    uint8_t *buf, size_t bufsz);
void sigqueue_init(struct dssh_signal_queue *q);
void sigqueue_free(struct dssh_signal_queue *q);
int sigqueue_push(struct dssh_signal_queue *q,
    const char *name, size_t stdout_pos, size_t stderr_pos);
bool sigqueue_ready(const struct dssh_signal_queue *q,
    size_t stdout_consumed, size_t stderr_consumed);
const char *sigqueue_pop(struct dssh_signal_queue *q,
    size_t stdout_consumed, size_t stderr_consumed,
    char *buf, size_t bufsz);
void acceptqueue_init(struct dssh_accept_queue *q);
void acceptqueue_free(struct dssh_accept_queue *q);
int acceptqueue_push(struct dssh_accept_queue *q,
    uint32_t peer_channel, uint32_t peer_window,
    uint32_t peer_max_packet,
    const uint8_t *type, size_t type_len);
struct dssh_incoming_open *acceptqueue_pop(struct dssh_accept_queue *q);

/*
 * ssh-conn.c internal functions exposed for testing.
 */
int send_data(dssh_session sess, struct dssh_channel_s *ch,
    const uint8_t *data, size_t len, size_t *sentp);
int send_extended_data(dssh_session sess, struct dssh_channel_s *ch,
    uint32_t data_type_code, const uint8_t *data, size_t len,
    size_t *sentp);
int send_eof(dssh_session sess, struct dssh_channel_s *ch);
int conn_close(dssh_session sess, struct dssh_channel_s *ch);
int send_window_adjust(dssh_session sess,
    struct dssh_channel_s *ch, uint32_t bytes);
int maybe_replenish_window(dssh_session sess, struct dssh_channel_s *ch);
int demux_dispatch(dssh_session sess, uint8_t msg_type,
    uint8_t *payload, size_t payload_len);
int parse_channel_request(const uint8_t *payload, size_t payload_len,
    const uint8_t **rtype, uint32_t *rtype_len,
    bool *want_reply,
    const uint8_t **req_data, size_t *req_data_len);

/*
 * ssh-trans.c internal functions exposed for testing.
 */
int parse_peer_kexinit(const uint8_t *buf, size_t bufsz,
    char lists[][1024], bool *first_kex_follows);
int encode_k_wire(const uint8_t *raw, size_t raw_sz,
    bool k_as_string, uint8_t **out, size_t *out_sz);
size_t first_name(const char *list, char *buf, size_t bufsz);

/*
 * Block size helpers from ssh-trans.c (BPP minimum clamping).
 */
size_t tx_block_size(dssh_session sess);
size_t rx_block_size(dssh_session sess);

/*
 * Test accessors for injecting version strings that bypass
 * set_version validation, allowing version_tx defense-in-depth
 * tests to reach the TOOLONG branches.
 */
void dssh_test_set_sw_version(const char *v);
void dssh_test_set_version_comment(const char *c);

/*
 * Version string validators from ssh-trans.c.
 */
bool has_nulls(uint8_t *buf, size_t buflen);
bool missing_crlf(uint8_t *buf, size_t buflen);
bool is_version_line(uint8_t *buf, size_t buflen);
bool has_non_ascii(uint8_t *buf, size_t buflen);
bool is_20(uint8_t *buf, size_t buflen);

/*
 * version_tx from ssh-trans.c -- sends the SSH version string.
 */
int version_tx(dssh_session sess);

/*
 * DH-GEX helpers from kex/dh-gex-sha256.c.
 */
#include <openssl/bn.h>

int64_t parse_bn_mpint(const uint8_t *buf, size_t bufsz, BIGNUM **bn);
int serialize_bn_mpint(const BIGNUM *bn, uint8_t *buf, size_t bufsz, size_t *pos);
bool dh_value_valid(const BIGNUM *val, const BIGNUM *p);
int compute_exchange_hash(
    const char *v_c, size_t v_c_len,
    const char *v_s, size_t v_s_len,
    const uint8_t *i_c, size_t i_c_len,
    const uint8_t *i_s, size_t i_s_len,
    const uint8_t *k_s, size_t k_s_len,
    uint32_t gex_min, uint32_t gex_n, uint32_t gex_max,
    const BIGNUM *p, const BIGNUM *g,
    const BIGNUM *e, const BIGNUM *f, const BIGNUM *k,
    uint8_t *hash_out);

/*
 * Curve25519 helpers from kex/curve25519-sha256.c.
 */
int compute_exchange_hash_c25519(
    const char *v_c, size_t v_c_len,
    const char *v_s, size_t v_s_len,
    const uint8_t *i_c, size_t i_c_len,
    const uint8_t *i_s, size_t i_s_len,
    const uint8_t *k_s, size_t k_s_len,
    const uint8_t *q_c, size_t q_c_len,
    const uint8_t *q_s, size_t q_s_len,
    const uint8_t *k_mpint, size_t k_mpint_len,
    uint8_t *hash_out);
int x25519_exchange(const uint8_t *peer_pub, size_t peer_pub_len,
    uint8_t *our_pub, uint8_t **secret, size_t *secret_len);
int encode_shared_secret(uint8_t *raw, size_t raw_len,
    uint8_t **ss_out, size_t *ss_len,
    uint8_t **mpint_out, size_t *mpint_len);
int curve25519_handler(struct dssh_kex_context *kctx);
int dhgex_handler(struct dssh_kex_context *kctx);
int sntrup761x25519_handler(struct dssh_kex_context *kctx);
int mlkem768x25519_handler(struct dssh_kex_context *kctx);

/*
 * ssh-auth.c helpers exposed for direct testing.
 */
int64_t parse_userauth_prefix(const uint8_t *payload, size_t payload_len,
    const uint8_t **user, size_t *user_len,
    const uint8_t **method, size_t *method_len);
#include "deucessh-auth.h"
int auth_server_impl(dssh_session sess,
    const struct dssh_auth_server_cbs *cbs,
    uint8_t *username_out, size_t *username_out_len);
int get_methods_impl(dssh_session sess,
    const char *username, char *methods, size_t methods_sz);
int auth_password_impl(dssh_session sess,
    const char *username, const char *password,
    dssh_auth_passwd_change_cb passwd_change_cb, void *cbdata);
int auth_kbi_impl(dssh_session sess,
    const char *username,
    dssh_auth_kbi_prompt_cb prompt_cb, void *cbdata);
int auth_publickey_impl(dssh_session sess,
    const char *username, const char *algo_name);

/*
 * ssh-trans.c DSSH_PRIVATE functions (still cross-file, need declarations).
 */
int send_packet(dssh_session sess,
    const uint8_t *payload, size_t payload_len, uint32_t *seq_out);
int recv_packet(dssh_session sess,
    uint8_t *msg_type, uint8_t **payload, size_t *payload_len);
int send_or_queue(dssh_session sess,
    const uint8_t *payload, size_t payload_len);

/*
 * ssh-trans.c DSSH_TESTABLE functions (visible under -DDSSH_TESTING).
 */
int version_exchange(dssh_session sess);
int kexinit(dssh_session sess);
int kex(dssh_session sess);
int newkeys(dssh_session sess);
int rekey(dssh_session sess);
bool rekey_needed(dssh_session sess);

/*
 * ssh.c internal function.
 */
void session_set_terminate(dssh_session sess);

#ifdef __cplusplus
}
#endif

#endif
