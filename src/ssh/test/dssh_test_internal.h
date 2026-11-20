/*
 * dssh_test_internal.h -- Declarations for functions exposed via
 * DSSH_TESTABLE when compiled with -DDSSH_TESTING.
 *
 * Only include this from test code, never from library code.
 */

#ifndef DSSH_TEST_INTERNAL_H
#define DSSH_TEST_INTERNAL_H

#include "deucessh.h"
#include "deucessh-crypto.h"
#include "ssh-internal.h"

struct dssh_channel_s;

#ifdef __cplusplus
extern "C" {
#endif

int demux_thread_func(void *arg);
void tx_wake_owners(dssh_session sess);
int dssh_test_unlock_boundary(mtx_t *mutex);
int drain_log_forwards(dssh_session sess);
int drain_one_wa_slot(dssh_session sess, struct dssh_channel_s *ch, bool *progress, bool discard);

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
int reserve_mac_buffers(dssh_session sess, uint16_t needed);

/*
 * Algorithm negotiation: returns the first mutually-supported algorithm
 * from client_list (preference order) that also appears in server_list
 * and is registered in the linked list starting at head.
 * name_offset is the byte offset of the name[] flex member in the struct.
 * next reads the declared linkage type and returns its converted value.
 * filter is an optional CSV whitelist (NULL = no filter).
 * Returns node pointer or NULL.
 */
void *negotiate_algo(struct dssh_name_list client_list,
    struct dssh_name_list server_list, void *head, size_t name_offset, dssh_registry_next next, const char *filter);

/*
 * Build a comma-separated name-list from a linked list of algorithm
 * entries.  head is the first node, name_offset is the offset of
 * the name[] flex member.  filter is an optional CSV whitelist
 * (NULL = registration-order, all entries).  Returns the length
 * written.
 */
int64_t build_namelist(void *head, size_t name_offset, dssh_registry_next next,
    char *buf, size_t bufsz, const char *filter);

void *registry_next_kex(void *node);
void *registry_next_key_algo(void *node);
void *registry_next_enc(void *node);
void *registry_next_mac(void *node);
void *registry_next_comp(void *node);

/*
 * Membership test: is `name` (length nlen) in the comma-separated
 * filter string?  Returns false when filter is NULL.
 */
bool name_in_filter(const char *name, size_t nlen, const char *filter);
int parse_ext_info(const uint8_t *payload, size_t length, bool client);
int build_server_ext_info(uint8_t *buf, size_t capacity, size_t *out_length);
int recv_packet_raw(dssh_session sess, uint8_t *msg_type, uint8_t **payload, size_t *payload_len);

/*
 * Global config -- DSSH_TESTABLE in ssh-trans.c.
 */
extern struct dssh_transport_global_config gconf;

/* Atomically arm a one-shot receive-counter value for the next packet
 * decoded by sess. */
void dssh_test_arm_rx_bytes_before_count(dssh_session sess, uint64_t value);

/* Successful complete rekey paths, excluding the initial handshake. The
 * counter saturates at UINT_MAX. Read only while the session remains owned. */
unsigned dssh_test_rekeys_completed(dssh_session sess);
void dssh_test_session_layout(size_t out[6]);

/* Current-thread, one-shot observation of detach waiting for active dispatch.
 * No status override or callback. Keep waiting alive until this thread joins;
 * disable explicitly if the close operation returns without observing it. */
void dssh_test_detach_observe(atomic_bool *waiting);

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
int event_queue_init(struct dssh_event_queue *q, size_t max_events, size_t packet_limit);
bool event_queue_freeze(struct dssh_event_queue *q);
void event_queue_free(struct dssh_event_queue *q);
int event_queue_push(struct dssh_event_queue *q,
    const struct dssh_event_entry *e);
void acceptqueue_init(struct dssh_accept_queue *q);
void acceptqueue_free(struct dssh_accept_queue *q);
int acceptqueue_push(struct dssh_accept_queue *q,
    uint32_t peer_channel, uint32_t peer_window,
    uint32_t peer_max_packet,
    const uint8_t *type, size_t type_len);
int acceptqueue_pop(struct dssh_accept_queue *q,
    struct dssh_incoming_open *out);

/*
 * ssh-conn.c internal functions exposed for testing.
 */
int tx_finalize(dssh_session sess, uint8_t *buf, size_t payload_len);
int drain_tx_slots(dssh_session sess);
int send_eof(dssh_session sess, struct dssh_channel_s *ch);
int conn_close(dssh_session sess, struct dssh_channel_s *ch);
int send_window_adjust(dssh_session sess,
    struct dssh_channel_s *ch, uint32_t bytes);
int maybe_replenish_window(dssh_session sess, struct dssh_channel_s *ch);
int dssh_test_demux_dispatch(dssh_session sess, uint8_t msg_type,
    uint8_t *payload, size_t payload_len);
int detach_channel(dssh_session sess, struct dssh_channel_s *ch);
int register_channel(dssh_session sess, struct dssh_channel_s *ch, bool *registered);
int64_t alloc_channel_id(dssh_session sess);
int accept_incoming(dssh_session sess, struct dssh_incoming_open *inc, int timeout_ms);
int wait_open_response(dssh_session sess, struct dssh_channel_s *ch);
struct dssh_channel_s *accept_channel_init(dssh_session sess, struct dssh_incoming_open *inc);
int wait_for_close(dssh_session sess, struct dssh_channel_s *ch);
int setup_release(dssh_session sess, struct dssh_channel_s *ch);
/* Borrows occupied channel storage until setup_release/final transition. */
int setup_recv(dssh_session sess, struct dssh_channel_s *ch, uint8_t *msg_type,
    uint8_t **payload, size_t *payload_len);
int derive_and_apply_keys(dssh_session sess, const uint8_t *k_mpint, size_t k_mpint_sz,
    dssh_enc old_enc_c2s, dssh_enc old_enc_s2c, dssh_mac old_mac_c2s, dssh_mac old_mac_s2c);
int wait_channel_request_response(dssh_session sess, struct dssh_channel_s *ch);
int send_channel_request_wait(dssh_session sess, struct dssh_channel_s *ch,
    const char *req_type, const uint8_t *extra, size_t extra_len);
int parse_channel_request(const uint8_t *payload, size_t payload_len,
    const uint8_t **rtype, uint32_t *rtype_len,
    bool *want_reply,
    const uint8_t **req_data, size_t *req_data_len);
struct dssh_chan_params;
struct dssh_chan_accept_cbs;
int accept_env_request(struct dssh_channel_s *ch, const uint8_t *data, size_t data_len,
    struct dssh_chan_params *p, const struct dssh_chan_accept_cbs *cbs, bool *cancelled);
int accept_parse_exec(const uint8_t *data, size_t data_len, struct dssh_chan_params *p);
int accept_parse_subsystem(const uint8_t *data, size_t data_len, struct dssh_chan_params *p);
int accept_parse_pty(const uint8_t *data, size_t data_len, struct dssh_chan_params *p);

/*
 * ssh-trans.c internal functions exposed for testing.
 */
int parse_peer_kexinit(const uint8_t *buf, size_t bufsz,
    struct dssh_name_list lists[10], bool *first_kex_follows);
int encode_k_wire(const uint8_t *raw, size_t raw_sz,
    bool k_as_string, uint8_t **out, size_t *out_sz);
size_t first_name(struct dssh_name_list list, char *buf, size_t bufsz);

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
 * DH-GEX shared handler from kex/dh-gex-sha256.c.
 * Bignum helpers are now static in the backend files.
 */

/*
 * Curve25519 helpers from kex/curve25519-sha256.c (shared protocol)
 * and kex/curve25519-sha256-{openssl,botan}.c (backend ops).
 */
struct hybrid_pq_params;
int hybrid_pq_compute_shared_secret(dssh_hash_ctx *hash, const struct hybrid_pq_params *p,
    const uint8_t *ss_pq, const uint8_t *ss_cl, uint8_t *out, size_t capacity);

int encode_shared_secret(const uint8_t *raw, size_t raw_len, struct dssh_bytes *secret,
    uint8_t *mpint, size_t capacity, size_t *mpint_len);

#ifdef DSSH_CRYPTO_OPENSSL
#include <openssl/bn.h>
/*
 * OpenSSL backend: DSSH_TESTABLE functions with original names.
 */
int curve25519_handler(const struct dssh_kex_context *kctx, struct dssh_kex_result *out);
int dhgex_handler(const struct dssh_kex_context *kctx, struct dssh_kex_result *out);
int dhgex_validate_group(const BIGNUM *p, const BIGNUM *g, BN_CTX *bnctx,
    const uint8_t *p_raw, size_t p_len, const uint8_t *g_raw, size_t g_len);
int sntrup761x25519_handler(const struct dssh_kex_context *kctx, struct dssh_kex_result *out);
#elif defined(DSSH_CRYPTO_BOTAN)
/*
 * Botan backend: extern "C" wrappers in the .cpp files.
 */
int dssh_botan_curve25519_handler(const struct dssh_kex_context *kctx, struct dssh_kex_result *out);
int dssh_botan_dhgex_handler(const struct dssh_kex_context *kctx, struct dssh_kex_result *out);
int dssh_botan_sntrup761x25519_handler(const struct dssh_kex_context *kctx, struct dssh_kex_result *out);
#define curve25519_handler       dssh_botan_curve25519_handler
#define dhgex_handler            dssh_botan_dhgex_handler
#define sntrup761x25519_handler  dssh_botan_sntrup761x25519_handler
#endif

/* Common ML-KEM handler and fixture generation for packet-replay tests. */
int mlkem768x25519_handler(const struct dssh_kex_context *kctx, struct dssh_kex_result *out);
int dssh_test_mlkem768_public_key(uint8_t *pk_out, size_t pk_len);

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
int auth_begin_impl(dssh_session sess, const char *username);
int64_t auth_get_methods_inner(dssh_session sess, char *methods, size_t capacity);
int auth_password_impl(dssh_session sess,
    const char *username, const char *password,
    dssh_auth_passwd_change_cb passwd_change_cb, void *cbdata);
int auth_kbi_impl(dssh_session sess,
    const char *username,
    dssh_auth_kbi_prompt_cb prompt_cb, void *cbdata);
int auth_publickey_impl(dssh_session sess,
    const char *username, const char *algo_name);
int handle_auth_publickey(dssh_session sess,
    const struct dssh_auth_server_cbs *cbs, const uint8_t *user,
    size_t user_len, const uint8_t *payload, size_t payload_len, size_t rpos);

/*
 * ssh-trans.c DSSH_PRIVATE functions (still cross-file, need declarations).
 */
int send_packet(dssh_session sess,
    const uint8_t *payload, size_t payload_len, uint32_t *seq_out);
int recv_packet(dssh_session sess,
    uint8_t *msg_type, uint8_t **payload, size_t *payload_len);
int resize_tx_slot_live(dssh_session sess, struct dssh_tx_slot *slot, size_t max_payload,
    size_t block_size, uint16_t mac_digest);
int send_to_slot(dssh_session sess, struct dssh_tx_slot *slot,
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
