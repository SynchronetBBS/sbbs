/* Copyright (C), 2026 by Stephen Hurd */

/*
 * conn_mqtt — minimal MQTT 5.0 client for connecting to a Synchronet
 * BBS's internal broker over TLS-PSK and bridging a node's terminal
 * stream into SyncTERM's normal conn_inbuf / conn_outbuf pipes.
 *
 * Subscribes to sbbs/{BBSID}/node/{N}/output (PUBLISH payloads → conn_inbuf)
 * and publishes keystrokes from conn_outbuf → sbbs/{BBSID}/node/{N}/input.
 *
 * BBSID and node number are auto-discovered before the live session begins:
 * BBSID by listening for a retained `sbbs/+/version` topic, node number
 * from a uifc picker populated by `sbbs/{BBSID}/node/+/status` retained
 * messages.  Both stages fall back to manual input boxes on timeout.
 *
 * The MQTT 5.0 codec is intentionally minimal: only the packet types we
 * actually exchange are implemented (CONNECT/CONNACK, SUBSCRIBE/SUBACK,
 * UNSUBSCRIBE/UNSUBACK, PUBLISH at QoS 0, PINGREQ/PINGRESP, DISCONNECT).
 * Property lists are skipped on receive and emitted as empty on send.
 */

#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "bbslist.h"
#include "ciolib.h"
#include "conn.h"
#include "conn_mqtt.h"
#include "genwrap.h"
#include "sockwrap.h"
#include "threadwrap.h"
#include "uifcinit.h"
#include "xp_tls.h"

/* ─────────────────────────────────────────────────────────── state */

static SOCKET            mqtt_sock = INVALID_SOCKET;
static xp_tls_t          mqtt_tls = NULL;
static pthread_mutex_t   mqtt_io_mutex;
static char              mqtt_bbsid[64];
static int               mqtt_node = 0;
static char              mqtt_input_topic[160];
static uint16_t          mqtt_pkt_id = 1;
static int               mqtt_keepalive_sec = 60;
static atomic_long       mqtt_last_send_us;

/* MQTT control packet types (high nibble of byte 0). */
#define MQTT_CONNECT      1
#define MQTT_CONNACK      2
#define MQTT_PUBLISH      3
#define MQTT_SUBSCRIBE    8
#define MQTT_SUBACK       9
#define MQTT_UNSUBSCRIBE 10
#define MQTT_UNSUBACK    11
#define MQTT_PINGREQ     12
#define MQTT_PINGRESP    13
#define MQTT_DISCONNECT  14

/* ──────────────────────────────────────────────────── varint codec */

/*
 * MQTT variable-byte integer.  1-4 bytes, max value 268,435,455.
 * Bit 7 of each byte is the continuation flag; bits 0-6 are payload.
 */
static size_t
encode_varint(uint8_t *dst, uint32_t val)
{
	size_t n = 0;
	do {
		uint8_t byte = val & 0x7f;
		val >>= 7;
		if (val)
			byte |= 0x80;
		dst[n++] = byte;
	} while (val && n < 4);
	return n;
}

/*
 * Decode varint.  On success returns 1 and fills *val and *consumed.
 * On underflow returns 0 (caller should accumulate more bytes).
 * On malformed varint (>4 bytes still continuing) returns -1.
 */
static int
decode_varint(const uint8_t *src, size_t len, uint32_t *val, size_t *consumed)
{
	uint32_t v = 0;
	uint32_t mult = 1;
	size_t i;
	for (i = 0; i < len && i < 4; i++) {
		v += (uint32_t)(src[i] & 0x7f) * mult;
		if ((src[i] & 0x80) == 0) {
			*val = v;
			*consumed = i + 1;
			return 1;
		}
		mult <<= 7;
	}
	if (i >= 4)
		return -1;
	return 0;
}

/* Append a 16-bit big-endian length-prefixed string to dst. */
static size_t
put_string(uint8_t *dst, const char *s)
{
	size_t len = s ? strlen(s) : 0;
	dst[0] = (uint8_t)(len >> 8);
	dst[1] = (uint8_t)len;
	if (len)
		memcpy(dst + 2, s, len);
	return len + 2;
}

/* ──────────────────────────────────────────────────── TLS I/O glue */

/*
 * Send `len` bytes over the TLS session, looping past partial writes.
 * Returns 0 on success, -1 on error (terminate flag also set).
 */
static int
tls_send_all(const void *buf, size_t len)
{
	const uint8_t *p = (const uint8_t *)buf;
	size_t total = 0;
	while (total < len) {
		size_t copied = 0;
		assert_pthread_mutex_lock(&mqtt_io_mutex);
		int rc = xp_tls_push(mqtt_tls, p + total, len - total, &copied);
		if (rc == XP_TLS_OK)
			(void)xp_tls_flush(mqtt_tls);
		assert_pthread_mutex_unlock(&mqtt_io_mutex);
		if (rc < 0)
			return -1;
		if (copied == 0 && rc == XP_TLS_OK)
			SLEEP(5);	/* spurious zero-progress; backoff */
		total += copied;
	}
	atomic_store(&mqtt_last_send_us, (long)(xp_timer() * 1e6));
	return 0;
}

/*
 * Pull bytes out of TLS and into rx_buf.  Returns the new buffer size,
 * or -1 on error.
 *
 * Both the TLS-pending check and socket_readable() run OUTSIDE the
 * mutex so the output thread can acquire it to publish keystrokes
 * while we're waiting on inbound traffic.  Either condition is enough
 * to drive a pop: TLS may have plaintext already buffered from a prior
 * record (xp_tls_has_pending) even when the kernel says there's no new
 * ciphertext on the socket.  Botan's TLS::Client itself isn't
 * thread-safe, so the lock is taken only for the actual xp_tls_pop
 * call, which returns without doing a blocking recv() since either
 * pending plaintext or kernel-buffered ciphertext is already there.
 */
static ssize_t
tls_pull(uint8_t *rx_buf, size_t rx_len, size_t rx_cap, int wait_ms)
{
	if (rx_len >= rx_cap)
		return rx_len;
	if (mqtt_sock == INVALID_SOCKET)
		return -1;
	if (!xp_tls_has_pending(mqtt_tls)
	    && !socket_readable(mqtt_sock, wait_ms))
		return rx_len;	/* nothing pending — caller can do other work */
	size_t got = 0;
	assert_pthread_mutex_lock(&mqtt_io_mutex);
	int rc = xp_tls_pop(mqtt_tls, rx_buf + rx_len, rx_cap - rx_len, &got);
	assert_pthread_mutex_unlock(&mqtt_io_mutex);
	if (rc == XP_TLS_TIMEOUT)
		return rx_len;	/* no progress, but not an error */
	if (rc < 0)
		return -1;
	return rx_len + got;
}

/*
 * Parse exactly one packet out of buf[0..len), returning (type, flags,
 * payload pointer, payload length, total bytes consumed).  Returns 0 if
 * the buffer doesn't yet hold a complete packet, -1 on protocol error,
 * 1 on success.
 */
static int
parse_packet(const uint8_t *buf, size_t len,
             uint8_t *type, uint8_t *flags,
             const uint8_t **payload, size_t *payload_len, size_t *consumed)
{
	if (len < 2)
		return 0;
	*type = (buf[0] >> 4) & 0x0f;
	*flags = buf[0] & 0x0f;
	uint32_t remaining;
	size_t var_used;
	int vrc = decode_varint(buf + 1, len - 1, &remaining, &var_used);
	if (vrc < 0)
		return -1;
	if (vrc == 0)
		return 0;
	size_t total = 1 + var_used + remaining;
	if (len < total)
		return 0;
	*payload = buf + 1 + var_used;
	*payload_len = remaining;
	*consumed = total;
	return 1;
}

/* ──────────────────────────────────────────── packet builders */

/*
 * Build CONNECT.  Returns total length written.  dst must be sized to
 * accommodate header + identity/credentials; 1 KiB is plenty for
 * BBS-typed fields.
 */
static size_t
build_connect(uint8_t *dst, const char *client_id,
              const char *username, const char *password,
              uint16_t keepalive)
{
	uint8_t var[1024];
	size_t v = 0;
	v += put_string(var + v, "MQTT");	/* protocol name */
	var[v++] = 5;				/* protocol version */
	uint8_t flags = 0x02;			/* clean start */
	if (username) flags |= 0x80;
	if (password) flags |= 0x40;
	var[v++] = flags;
	var[v++] = (uint8_t)(keepalive >> 8);
	var[v++] = (uint8_t)keepalive;
	var[v++] = 0;	/* property length 0 */

	/* payload */
	v += put_string(var + v, client_id ? client_id : "");
	if (username)
		v += put_string(var + v, username);
	if (password)
		v += put_string(var + v, password);

	dst[0] = (MQTT_CONNECT << 4);
	size_t hl = 1 + encode_varint(dst + 1, (uint32_t)v);
	memcpy(dst + hl, var, v);
	return hl + v;
}

/*
 * SUBSCRIBE for one topic filter at QoS 0.  Returns total length.
 * Caller supplies packet ID.
 */
static size_t
build_subscribe(uint8_t *dst, uint16_t pkt_id, const char *topic_filter)
{
	uint8_t var[256];
	size_t v = 0;
	var[v++] = (uint8_t)(pkt_id >> 8);
	var[v++] = (uint8_t)pkt_id;
	var[v++] = 0;	/* property length 0 */
	v += put_string(var + v, topic_filter);
	var[v++] = 0;	/* subscription options: QoS 0 */

	dst[0] = (MQTT_SUBSCRIBE << 4) | 0x02;
	size_t hl = 1 + encode_varint(dst + 1, (uint32_t)v);
	memcpy(dst + hl, var, v);
	return hl + v;
}

static size_t
build_unsubscribe(uint8_t *dst, uint16_t pkt_id, const char *topic_filter)
{
	uint8_t var[256];
	size_t v = 0;
	var[v++] = (uint8_t)(pkt_id >> 8);
	var[v++] = (uint8_t)pkt_id;
	var[v++] = 0;	/* property length 0 */
	v += put_string(var + v, topic_filter);

	dst[0] = (MQTT_UNSUBSCRIBE << 4) | 0x02;
	size_t hl = 1 + encode_varint(dst + 1, (uint32_t)v);
	memcpy(dst + hl, var, v);
	return hl + v;
}

/*
 * PUBLISH at QoS 0 (no packet ID).  Caller supplies the topic and the
 * payload.  Caller's buffer must hold header + 4 (topic length) + topic
 * + 1 (property length) + payload.
 */
static size_t
build_publish(uint8_t *dst, size_t dst_cap,
              const char *topic, const void *payload, size_t payload_len)
{
	size_t topic_len = strlen(topic);
	size_t var_len = 2 + topic_len + 1 + payload_len;
	if (var_len > dst_cap - 5)
		return 0;
	uint8_t *var = dst + 5;	/* leave room for fixed header */
	size_t v = 0;
	v += put_string(var + v, topic);
	var[v++] = 0;	/* property length 0 */
	if (payload_len)
		memcpy(var + v, payload, payload_len);
	v += payload_len;

	dst[0] = (MQTT_PUBLISH << 4);	/* QoS 0, no retain, no dup */
	size_t hl = 1 + encode_varint(dst + 1, (uint32_t)v);
	memmove(dst + hl, var, v);
	return hl + v;
}

static size_t
build_pingreq(uint8_t *dst)
{
	dst[0] = (MQTT_PINGREQ << 4);
	dst[1] = 0;
	return 2;
}

static size_t
build_disconnect(uint8_t *dst)
{
	dst[0] = (MQTT_DISCONNECT << 4);
	dst[1] = 0;
	return 2;
}

/* ──────────────────────────────────────────── packet senders */

static int
send_connect(const char *user, const char *syspass)
{
	uint8_t buf[1024];
	char client_id[40];
	snprintf(client_id, sizeof(client_id), "syncterm-%u", (unsigned)getpid());
	size_t n = build_connect(buf, client_id, user, syspass, mqtt_keepalive_sec);
	return tls_send_all(buf, n);
}

static int
send_subscribe(const char *topic_filter)
{
	uint8_t buf[512];
	uint16_t id = mqtt_pkt_id++;
	if (mqtt_pkt_id == 0) mqtt_pkt_id = 1;	/* MQTT requires non-zero */
	size_t n = build_subscribe(buf, id, topic_filter);
	return tls_send_all(buf, n);
}

static int
send_unsubscribe(const char *topic_filter)
{
	uint8_t buf[512];
	uint16_t id = mqtt_pkt_id++;
	if (mqtt_pkt_id == 0) mqtt_pkt_id = 1;
	size_t n = build_unsubscribe(buf, id, topic_filter);
	return tls_send_all(buf, n);
}

static int
send_publish(const char *topic, const void *payload, size_t payload_len)
{
	uint8_t buf[1024];
	if (payload_len + strlen(topic) + 16 > sizeof(buf))
		return -1;
	size_t n = build_publish(buf, sizeof(buf), topic, payload, payload_len);
	if (n == 0)
		return -1;
	return tls_send_all(buf, n);
}

/* ──────────────────────────────────────────── PUBLISH parser */

/*
 * Parse the variable header of an inbound PUBLISH at QoS 0:
 * topic name, property length (skipped), payload pointer.
 * Returns 0 on success, -1 on malformed.
 */
static int
parse_publish(uint8_t flags, const uint8_t *p, size_t len,
              const uint8_t **topic_p, size_t *topic_len_p,
              const uint8_t **payload_p, size_t *payload_len_p)
{
	if (len < 2)
		return -1;
	size_t tl = ((size_t)p[0] << 8) | p[1];
	if (tl + 2 > len)
		return -1;
	*topic_p = p + 2;
	*topic_len_p = tl;
	const uint8_t *q = p + 2 + tl;
	size_t qlen = len - 2 - tl;
	/* QoS in flags bits 1-2 */
	uint8_t qos = (flags >> 1) & 0x03;
	if (qos > 0) {
		if (qlen < 2)
			return -1;
		q += 2;
		qlen -= 2;
	}
	uint32_t prop_len;
	size_t prop_var;
	int vrc = decode_varint(q, qlen, &prop_len, &prop_var);
	if (vrc <= 0)
		return -1;
	if (prop_var + prop_len > qlen)
		return -1;
	q += prop_var + prop_len;
	qlen -= prop_var + prop_len;
	*payload_p = q;
	*payload_len_p = qlen;
	return 0;
}

/* ──────────────────────────────────────────── discovery */

#define MAX_DISCOVERED_NODES 64

struct discovered_node {
	int  number;
	char status[64];	/* raw "%u\t%u\t%u\t..." snippet */
};

struct discovery_state {
	bool                   want_bbsid;
	bool                   want_nodes;
	int                    node_count;
	struct discovered_node nodes[MAX_DISCOVERED_NODES];
};

/*
 * Pull from TLS into rx_buf for up to deadline_sec seconds, dispatching
 * each PUBLISH to the discovery callback.
 */
static int
discovery_pump(struct discovery_state *st, double deadline_sec)
{
	uint8_t  rx_buf[8192];
	size_t   rx_len = 0;
	double   end = xp_timer() + deadline_sec;

	while (xp_timer() < end) {
		ssize_t got = tls_pull(rx_buf, rx_len, sizeof(rx_buf), 100);
		if (got < 0)
			return -1;
		rx_len = (size_t)got;

		for (;;) {
			uint8_t type, flags;
			const uint8_t *payload;
			size_t payload_len, consumed;
			int prc = parse_packet(rx_buf, rx_len, &type, &flags,
			                       &payload, &payload_len, &consumed);
			if (prc <= 0)
				break;

			if (type == MQTT_PUBLISH) {
				const uint8_t *topic_b;
				size_t topic_l;
				const uint8_t *body;
				size_t body_l;
				if (parse_publish(flags, payload, payload_len,
				                  &topic_b, &topic_l,
				                  &body, &body_l) == 0) {
					/* BBSID extraction: sbbs/{BBSID} (two-segment, no
					   trailing slash, retained at server startup) or
					   any deeper sbbs/{BBSID}/... topic.  In both
					   cases the BBSID is the bytes between "sbbs/"
					   and the next '/' (or end of topic). */
					if (st->want_bbsid && topic_l > 5
					    && memcmp(topic_b, "sbbs/", 5) == 0) {
						const uint8_t *q = topic_b + 5;
						size_t r = topic_l - 5;
						const uint8_t *slash = memchr(q, '/', r);
						size_t n = slash ? (size_t)(slash - q) : r;
						if (n > 0 && n < sizeof(mqtt_bbsid)) {
							memcpy(mqtt_bbsid, q, n);
							mqtt_bbsid[n] = 0;
							st->want_bbsid = false;
						}
					}
					/* Node parsing: topic must look like
					   sbbs/{BBSID}/node/{N}/status */
					if (st->want_nodes && mqtt_bbsid[0] != 0) {
						char tmp[256];
						if (topic_l < sizeof(tmp)) {
							memcpy(tmp, topic_b, topic_l);
							tmp[topic_l] = 0;
							size_t pl = strlen("sbbs/") + strlen(mqtt_bbsid)
							          + strlen("/node/");
							char prefix[128];
							snprintf(prefix, sizeof(prefix),
							         "sbbs/%s/node/", mqtt_bbsid);
							if (strncmp(tmp, prefix, strlen(prefix)) == 0) {
								int num = atoi(tmp + strlen(prefix));
								const char *tail = strchr(tmp + strlen(prefix), '/');
								if (num > 0 && tail != NULL
								    && strcmp(tail, "/status") == 0
								    && st->node_count < MAX_DISCOVERED_NODES) {
									/* Dedup */
									bool seen = false;
									for (int j = 0; j < st->node_count; j++) {
										if (st->nodes[j].number == num) {
											seen = true;
											break;
										}
									}
									if (!seen) {
										struct discovered_node *dn = &st->nodes[st->node_count++];
										dn->number = num;
										size_t cl = body_l < sizeof(dn->status) - 1
										          ? body_l : sizeof(dn->status) - 1;
										memcpy(dn->status, body, cl);
										dn->status[cl] = 0;
									}
								}
							}
							(void)pl;
						}
					}
				}
			}
			/* SUBACK / UNSUBACK / PINGRESP / DISCONNECT — ignored
			   during discovery; we don't track packet IDs here. */

			if (consumed >= rx_len) {
				rx_len = 0;
			}
			else {
				memmove(rx_buf, rx_buf + consumed, rx_len - consumed);
				rx_len -= consumed;
			}
		}
	}
	return 0;
}

/*
 * Sort discovered nodes by node number ascending.
 */
static int
node_cmp(const void *a, const void *b)
{
	int na = ((const struct discovered_node *)a)->number;
	int nb = ((const struct discovered_node *)b)->number;
	return na - nb;
}

/*
 * Map node_status enum value to a short label.  Mirrors values from
 * sbbs3/nodedefs.h NODE_WFC..NODE_OFFLINE.
 */
static const char *
node_status_label(int status)
{
	switch (status) {
		case 0:  return "WFC";
		case 1:  return "logon";
		case 2:  return "newuser";
		case 3:  return "main";
		case 4:  return "xfer";
		case 5:  return "msg";
		case 6:  return "ext";
		case 7:  return "in use";
		case 8:  return "quiet";
		case 9:  return "logout";
		default: return "?";
	}
}

/* ──────────────────────────────────────────── pickers */

static int
prompt_node_number(struct bbslist *bbs)
{
	char in[8] = "1";
	uifc.helpbuf =
	    "`Node Number`\n\n"
	    "Enter the BBS node number to connect to.";
	int rc = uifc.input(WIN_MID | WIN_SAV, 0, 0, "Node Number",
	                    in, sizeof(in) - 1, K_NUMBER | K_EDIT);
	(void)bbs;
	if (rc < 0)
		return -1;
	int n = atoi(in);
	if (n < 1)
		return -1;
	return n;
}

static int
prompt_bbsid(void)
{
	uifc.helpbuf =
	    "`BBS Identifier`\n\n"
	    "Enter the BBSID (QWK ID) of the BBS.  This is the upper-case\n"
	    "short identifier configured in scfg under 'System Information'.";
	int rc = uifc.input(WIN_MID | WIN_SAV, 0, 0, "BBSID",
	                    mqtt_bbsid, sizeof(mqtt_bbsid) - 1, K_EDIT | K_UPPER);
	if (rc < 0 || mqtt_bbsid[0] == 0)
		return -1;
	return 0;
}

/*
 * Show a uifc.list of discovered nodes; returns chosen node number, or
 * 0 if the user cancelled, or -1 on error.  If no nodes were
 * discovered, falls through to a manual input prompt.
 */
static int
pick_node(struct discovery_state *st, struct bbslist *bbs)
{
	if (st->node_count == 0)
		return prompt_node_number(bbs);

	qsort(st->nodes, st->node_count, sizeof(st->nodes[0]), node_cmp);

	char  buf[MAX_DISCOVERED_NODES + 1][80];
	char *opts[MAX_DISCOVERED_NODES + 2];
	int   n = 0;
	for (int i = 0; i < st->node_count; i++) {
		int status = atoi(st->nodes[i].status);
		snprintf(buf[n], sizeof(buf[n]),
		         "Node %d  (%s)",
		         st->nodes[i].number,
		         node_status_label(status));
		opts[n] = buf[n];
		n++;
	}
	snprintf(buf[n], sizeof(buf[n]), "(Enter node number manually)");
	opts[n] = buf[n];
	n++;
	opts[n] = NULL;

	int picked = 0;
	uifc.helpbuf =
	    "`Pick a Node`\n\n"
	    "Choose a node to connect to.  The list shows all nodes the\n"
	    "broker has retained state for; pick one to attach to its\n"
	    "input/output stream.";
	int rc = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &picked, NULL,
	                   "Active Nodes", opts);
	if (rc < 0)
		return 0;
	if (picked == st->node_count)
		return prompt_node_number(bbs);
	return st->nodes[picked].number;
}

/* ──────────────────────────────────────────── live threads */

static void
mqtt_input_thread(void *args)
{
	(void)args;
	uint8_t rx_buf[16384];
	size_t  rx_len = 0;
	char    output_topic[160];
	snprintf(output_topic, sizeof(output_topic),
	         "sbbs/%s/node/%d/output", mqtt_bbsid, mqtt_node);
	size_t  out_topic_len = strlen(output_topic);

	SetThreadName("MQTT Input");
	conn_api.input_thread_running = 1;

	while (!conn_api.terminate) {
		ssize_t got = tls_pull(rx_buf, rx_len, sizeof(rx_buf), 100);
		if (got < 0)
			break;
		rx_len = (size_t)got;

		for (;;) {
			uint8_t type, flags;
			const uint8_t *payload;
			size_t payload_len, consumed;
			int prc = parse_packet(rx_buf, rx_len, &type, &flags,
			                       &payload, &payload_len, &consumed);
			if (prc <= 0)
				break;

			if (type == MQTT_PUBLISH) {
				const uint8_t *topic_b;
				size_t topic_l;
				const uint8_t *body;
				size_t body_l;
				if (parse_publish(flags, payload, payload_len,
				                  &topic_b, &topic_l,
				                  &body, &body_l) == 0
				    && topic_l == out_topic_len
				    && memcmp(topic_b, output_topic, out_topic_len) == 0) {
					/* Push payload bytes into conn_inbuf, blocking
					   if the buffer is temporarily full. */
					size_t off = 0;
					while (off < body_l && !conn_api.terminate) {
						assert_pthread_mutex_lock(&(conn_inbuf.mutex));
						conn_buf_wait_free(&conn_inbuf, 1, 1000);
						size_t put = conn_buf_put(&conn_inbuf,
						    body + off, body_l - off);
						assert_pthread_mutex_unlock(&(conn_inbuf.mutex));
						off += put;
					}
				}
			}
			else if (type == MQTT_DISCONNECT) {
				conn_api.terminate = true;
				break;
			}

			if (consumed >= rx_len) {
				rx_len = 0;
			}
			else {
				memmove(rx_buf, rx_buf + consumed, rx_len - consumed);
				rx_len -= consumed;
			}
		}
	}
	conn_api.terminate = true;
	conn_api.input_thread_running = 2;
}

static void
mqtt_output_thread(void *args)
{
	(void)args;
	uint8_t  buf[1024];
	long     last_send_us = 0;

	SetThreadName("MQTT Output");
	conn_api.output_thread_running = 1;

	while (!conn_api.terminate) {
		assert_pthread_mutex_lock(&(conn_outbuf.mutex));
		size_t wr = conn_buf_wait_bytes(&conn_outbuf, 1, 100);
		if (wr) {
			/* Cap each PUBLISH at 512 bytes payload to keep the
			   stack buffer small and TLS records bite-sized. */
			if (wr > 512)
				wr = 512;
			wr = conn_buf_get(&conn_outbuf, buf, wr);
		}
		assert_pthread_mutex_unlock(&(conn_outbuf.mutex));

		if (wr) {
			if (send_publish(mqtt_input_topic, buf, wr) < 0) {
				conn_api.terminate = true;
				break;
			}
		}
		else {
			/* No outbound data — check keepalive.  Send PINGREQ
			   when half the keepalive interval has elapsed since
			   the last write. */
			long now_us = (long)(xp_timer() * 1e6);
			long since = now_us - atomic_load(&mqtt_last_send_us);
			if (since > (long)mqtt_keepalive_sec * 500000) {
				uint8_t ping[2];
				size_t n = build_pingreq(ping);
				if (tls_send_all(ping, n) < 0) {
					conn_api.terminate = true;
					break;
				}
			}
			(void)last_send_us;
		}
	}
	conn_api.terminate = true;
	conn_api.output_thread_running = 2;
}

/* ──────────────────────────────────────────── connect / close */

/*
 * Lowercase a fresh stack copy of `s` into `dst` (size cap).  Used to
 * derive the TLS-PSK identity from bbs->user the same way sbbs3 does.
 */
static void
lower_copy(char *dst, size_t cap, const char *src)
{
	size_t i = 0;
	while (i + 1 < cap && src[i] != 0) {
		unsigned char c = (unsigned char)src[i];
		if (c >= 'A' && c <= 'Z')
			c += 'a' - 'A';
		dst[i] = (char)c;
		i++;
	}
	dst[i] = 0;
}

int
mqtt_connect(struct bbslist *bbs)
{
	char id_buf[MAX_USER_LEN + 1];
	char psk_buf[MAX_PASSWD_LEN + 1];
	const char *fail_reason = NULL;

	if (!bbs->hidepopups)
		init_uifc(true, true);
	assert_pthread_mutex_init(&mqtt_io_mutex, NULL);


	mqtt_bbsid[0] = 0;
	mqtt_node = 0;
	mqtt_pkt_id = 1;
	atomic_store(&mqtt_last_send_us, (long)(xp_timer() * 1e6));

	mqtt_sock = conn_socket_connect(bbs, true);
	if (mqtt_sock == INVALID_SOCKET)
		return -1;

	/* Disable Nagle so single-byte keystroke PUBLISH packets don't
	 * sit in the TCP send buffer waiting for more data or an ACK.
	 * Without this, each keystroke is held back until the next event
	 * arrives — typing feels broken because the BBS sees keys one
	 * behind the user. */
	int off = 1;
	if (setsockopt(mqtt_sock, IPPROTO_TCP, TCP_NODELAY,
	    (char *)&off, sizeof(off)) != 0) {
		fprintf(stderr, "%s:%d: setsockopt TCP_NODELAY failed (%d)\n",
		    __FILE__, __LINE__, errno);
	}

	if (!bbs->hidepopups)
		uifc.pop("Negotiating TLS-PSK");

	lower_copy(id_buf, sizeof(id_buf), bbs->user);
	lower_copy(psk_buf, sizeof(psk_buf), bbs->password);
	mqtt_tls = xp_tls_client_open_psk(mqtt_sock, bbs->addr, 1,
	                                  id_buf,
	                                  psk_buf, strlen(psk_buf));
	/* psk_buf is a stack copy; xp_tls_client_open_psk made an internal
	   secure copy.  Wipe ours before it goes out of scope. */
	memset(psk_buf, 0, sizeof(psk_buf));

	/* If PSK didn't work, fall back to a plain cert handshake.  The
	 * broker is likely an external one (mosquitto/EMQX/etc.) that
	 * doesn't speak TLS-PSK at all; we still authenticate at the MQTT
	 * layer with bbs->user / bbs->password.
	 *
	 * Reopen the socket: a failed TLS handshake leaves it in an
	 * indeterminate state (peer may have sent a fatal alert) so the
	 * cleanest thing is to drop it and dial again. */
	char psk_err[256];
	psk_err[0] = 0;
	if (mqtt_tls == NULL) {
		strncpy(psk_err, xp_tls_last_err(), sizeof(psk_err) - 1);
		psk_err[sizeof(psk_err) - 1] = 0;

		closesocket(mqtt_sock);
		mqtt_sock = INVALID_SOCKET;

		if (!bbs->hidepopups) {
			uifc.pop(NULL);
			uifc.pop("Negotiating TLS (cert)");
		}
		mqtt_sock = conn_socket_connect(bbs, true);
		if (mqtt_sock == INVALID_SOCKET) {
			if (!bbs->hidepopups) {
				uifc.pop(NULL);
				char str[600];
				snprintf(str, sizeof(str),
				    "TLS-PSK failed: %s\nTCP reconnect for cert retry failed.",
				    psk_err);
				uifcmsg("MQTT TLS Error", str);
			}
			conn_api.terminate = true;
			return -1;
		}
		if (setsockopt(mqtt_sock, IPPROTO_TCP, TCP_NODELAY,
		    (char *)&off, sizeof(off)) != 0) {
			fprintf(stderr, "%s:%d: setsockopt TCP_NODELAY failed (%d)\n",
			    __FILE__, __LINE__, errno);
		}
		mqtt_tls = xp_tls_client_open(mqtt_sock, bbs->addr, 1);
	}

	if (mqtt_tls == NULL) {
		if (!bbs->hidepopups) {
			uifc.pop(NULL);
			char str[800];
			if (psk_err[0])
				snprintf(str, sizeof(str),
				    "TLS handshake failed.\nPSK leg: %s\nCert leg: %s",
				    psk_err, xp_tls_last_err());
			else
				snprintf(str, sizeof(str),
				    "TLS handshake failed: %s", xp_tls_last_err());
			uifcmsg("MQTT TLS Error", str);
		}
		closesocket(mqtt_sock);
		mqtt_sock = INVALID_SOCKET;
		conn_api.terminate = true;
		return -1;
	}

	if (!bbs->hidepopups) {
		uifc.pop(NULL);
		uifc.pop("Sending CONNECT");
	}
	/* Pick MQTT-level credentials based on which TLS kex actually
	 * negotiated.  PSK path = Synchronet internal broker, MQTT password
	 * is the system password.  Non-PSK path = external broker that's
	 * still serving Synchronet-shape topics but uses a normal
	 * username/password challenge (whatever the operator configured),
	 * MQTT password is just bbs->password.
	 *
	 * The system-password slot only applies on the PSK path, so we
	 * only prompt for it after the handshake — and only if it's
	 * actually needed.  Prompting before would harass cert-broker
	 * users for a value they never use. */
	bool used_psk = xp_tls_used_psk(mqtt_tls);
	if (used_psk && bbs->syspass[0] == 0 && !bbs->hidepopups) {
		uifc.pop(NULL);
		uifc.helpbuf =
		    "`System Password`\n\n"
		    "The Synchronet internal MQTT broker authenticates the\n"
		    "connection with the BBS's system password.  Enter it here.";
		int rc = uifc.input(WIN_MID | WIN_SAV, 0, 0, "System Password",
		                    bbs->syspass, sizeof(bbs->syspass) - 1,
		                    K_EDIT | K_PASSWORD);
		if (rc < 0 || bbs->syspass[0] == 0) {
			fail_reason = "no system password supplied";
			goto fail;
		}
		uifc.pop("Sending CONNECT");
	}
	const char *mqtt_pw = used_psk ? bbs->syspass : bbs->password;
	if (send_connect(bbs->user, mqtt_pw) < 0) {
		fail_reason = "send_connect: TLS write failed";
		goto fail;
	}

	/* Wait for CONNACK */
	uint8_t connack_buf[256];
	size_t  connack_len = 0;
	double  deadline = xp_timer() + 5.0;
	uint8_t type = 0, flags = 0;
	const uint8_t *cak_payload = NULL;
	size_t  cak_plen = 0, cak_used = 0;
	int got_connack = 0;
	static char connack_err[400];
	while (xp_timer() < deadline) {
		ssize_t g = tls_pull(connack_buf, connack_len, sizeof(connack_buf), 100);
		if (g < 0) {
			/* Broker dropped the TCP after consuming our CONNECT but
			   before sending CONNACK -- almost always means parse_connect
			   on its end rejected our packet (it tears down without
			   sending CONNACK on parse failure). */
			snprintf(connack_err, sizeof(connack_err),
			    "Broker closed the connection before sending CONNACK.\n"
			    "Most likely our CONNECT packet failed the broker's parser.\n"
			    "TLS layer says: %s",
			    xp_tls_errstr(mqtt_tls));
			fail_reason = connack_err;
			goto fail;
		}
		connack_len = (size_t)g;
		int prc = parse_packet(connack_buf, connack_len, &type, &flags,
		                       &cak_payload, &cak_plen, &cak_used);
		if (prc < 0) {
			fail_reason = "CONNACK parse: malformed packet from broker";
			goto fail;
		}
		if (prc == 1) {
			got_connack = 1;
			break;
		}
	}
	if (!got_connack || type != MQTT_CONNACK || cak_plen < 2) {
		if (!bbs->hidepopups) {
			uifc.pop(NULL);
			uifcmsg("MQTT Error", "No CONNACK received from broker.");
		}
		goto fail;
	}
	if (cak_payload[1] != 0) {
		if (!bbs->hidepopups) {
			char str[128];
			snprintf(str, sizeof(str),
			         "Broker rejected CONNECT (reason 0x%02X)",
			         cak_payload[1]);
			uifc.pop(NULL);
			uifcmsg("MQTT Auth Error", str);
		}
		goto fail;
	}

	/* BBSID discovery — the BBS publishes sbbs/{BBSID} (retained, value
	   = sys_name) at server startup, so a wildcard subscribe at the
	   second level returns one retained PUBLISH per BBSID the broker
	   knows about. */
	if (!bbs->hidepopups) {
		uifc.pop(NULL);
		uifc.pop("Discovering BBSID");
	}
	if (send_subscribe("sbbs/+") < 0) {
		fail_reason = "subscribe(sbbs/+): TLS write failed";
		goto fail;
	}

	struct discovery_state st = { .want_bbsid = true };
	if (discovery_pump(&st, 1.0) < 0) {
		fail_reason = "BBSID discovery pump failed";
		goto fail;
	}

	if (mqtt_bbsid[0] == 0) {
		if (!bbs->hidepopups)
			uifc.pop(NULL);
		if (prompt_bbsid() < 0) {
			if (!bbs->hidepopups)
				uifcmsg("MQTT Error",
				    "No BBSID was discovered and none was entered.\n"
				    "The broker has no retained `sbbs/{BBSID}` topic, which\n"
				    "means the BBS has not yet published its identity to MQTT.");
			goto fail;
		}
		if (!bbs->hidepopups)
			uifc.pop("Discovering nodes");
	}
	else if (!bbs->hidepopups) {
		uifc.pop(NULL);
		uifc.pop("Discovering nodes");
	}

	(void)send_unsubscribe("sbbs/+");

	/* Node discovery */
	char node_filter[160];
	snprintf(node_filter, sizeof(node_filter),
	         "sbbs/%s/node/+/status", mqtt_bbsid);
	if (send_subscribe(node_filter) < 0) {
		fail_reason = "subscribe(node/+/status): TLS write failed";
		goto fail;
	}

	st.want_bbsid = false;
	st.want_nodes = true;
	if (discovery_pump(&st, 1.0) < 0) {
		fail_reason = "node discovery pump failed";
		goto fail;
	}

	if (!bbs->hidepopups)
		uifc.pop(NULL);

	mqtt_node = pick_node(&st, bbs);
	if (mqtt_node <= 0) {
		if (!bbs->hidepopups)
			uifcmsg("MQTT",
			    "No node selected.  Cancelled.");
		goto fail;
	}

	(void)send_unsubscribe(node_filter);

	/* Live mode */
	char output_filter[160];
	snprintf(output_filter, sizeof(output_filter),
	         "sbbs/%s/node/%d/output", mqtt_bbsid, mqtt_node);
	snprintf(mqtt_input_topic, sizeof(mqtt_input_topic),
	         "sbbs/%s/node/%d/input", mqtt_bbsid, mqtt_node);
	if (send_subscribe(output_filter) < 0) {
		fail_reason = "subscribe(output): TLS write failed";
		goto fail;
	}

	if (!create_conn_buf(&conn_inbuf, BUFFER_SIZE)) {
		fail_reason = "create_conn_buf(in) failed (out of memory)";
		goto fail;
	}
	if (!create_conn_buf(&conn_outbuf, BUFFER_SIZE)) {
		destroy_conn_buf(&conn_inbuf);
		fail_reason = "create_conn_buf(out) failed (out of memory)";
		goto fail;
	}

	_beginthread(mqtt_output_thread, 0, NULL);
	_beginthread(mqtt_input_thread, 0, NULL);

	/* Verify the threads survive past the broker's first chance to
	   reject us.  If the broker disconnected after our SUBSCRIBE
	   (malformed packet, ACL deny, etc.), the input thread's first
	   xp_tls_pop sees a closed socket and exits.  Without this check
	   mqtt_connect returns 0, conn_connect() waits for running==1,
	   sees running==2 and unwinds back to the bbslist with no error
	   ever surfaced.  Capture xp_tls_errstr while the session is
	   still alive so we can show what actually went wrong. */
	for (int waited = 0; waited < 400; waited += 20) {
		if (conn_api.input_thread_running == 1
		    && conn_api.output_thread_running == 1)
			break;
		if (conn_api.input_thread_running == 2
		    || conn_api.output_thread_running == 2)
			break;
		SLEEP(20);
	}
	if (conn_api.input_thread_running == 2
	    || conn_api.output_thread_running == 2) {
		static char tls_err[300];
		const char *e = mqtt_tls != NULL ? xp_tls_errstr(mqtt_tls) : "session torn down";
		snprintf(tls_err, sizeof(tls_err),
		         "Broker dropped the connection after our SUBSCRIBE.\n%s", e);
		fail_reason = tls_err;
		conn_api.terminate = true;
		while (conn_api.input_thread_running == 1
		    || conn_api.output_thread_running == 1)
			SLEEP(1);
		goto fail;
	}

	if (!bbs->hidepopups)
		uifc.pop(NULL);
	return 0;

fail:
	if (!bbs->hidepopups)
		uifc.pop(NULL);
	if (!bbs->hidepopups && fail_reason != NULL)
		uifcmsg("MQTT Error", (char *)fail_reason);
	if (mqtt_tls != NULL) {
		xp_tls_close(mqtt_tls, false);
		mqtt_tls = NULL;
	}
	if (mqtt_sock != INVALID_SOCKET) {
		closesocket(mqtt_sock);
		mqtt_sock = INVALID_SOCKET;
	}
	conn_api.terminate = true;
	return -1;
}

int
mqtt_close(void)
{
	char garbage[1024];
	conn_api.terminate = true;

	/* Best-effort DISCONNECT — only if the session is still alive. */
	if (mqtt_tls != NULL) {
		uint8_t buf[2];
		size_t  n = build_disconnect(buf);
		(void)tls_send_all(buf, n);
	}

	if (mqtt_sock != INVALID_SOCKET)
		shutdown(mqtt_sock, SHUT_RDWR);
	while (conn_api.input_thread_running == 1
	    || conn_api.output_thread_running == 1) {
		conn_recv_upto(garbage, sizeof(garbage), 0);
		SLEEP(1);
	}
	if (mqtt_tls != NULL) {
		xp_tls_close(mqtt_tls, false);
		mqtt_tls = NULL;
	}
	if (mqtt_sock != INVALID_SOCKET) {
		closesocket(mqtt_sock);
		mqtt_sock = INVALID_SOCKET;
	}
	destroy_conn_buf(&conn_inbuf);
	destroy_conn_buf(&conn_outbuf);
	pthread_mutex_destroy(&mqtt_io_mutex);
	return 0;
}
