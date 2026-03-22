# RFC 4253 Conformance Audit — DeuceSSH

Systematic audit of every MUST, MUST NOT, REQUIRED, SHALL, SHALL NOT,
SHOULD, SHOULD NOT, RECOMMENDED, MAY, and OPTIONAL keyword in RFC 4253
(SSH Transport Layer Protocol) against the DeuceSSH library code.

Legend:
- **CONFORMS** — library satisfies the requirement
- **N/A** — requirement applies to a layer/role not implemented by this library
- **PARTIAL** — partially conformant, see notes
- **NONCONFORMANT** — library violates the requirement
- **APPLICATION** — requirement is the responsibility of the application using the library

---

## Section 4.2: Protocol Version Exchange

### 4.2-1
> When the connection has been established, both sides **MUST** send an
> identification string.

**CONFORMS** — `deuce_ssh_transport_version_exchange()` calls
`version_tx()` then `version_rx()`.  Both sides always send their
version string.

### 4.2-2
> This identification string **MUST** be:
> `SSH-protoversion-softwareversion SP comments CR LF`

**CONFORMS** — `version_tx()` (`ssh-trans.c:124`) builds the string as
`"SSH-2.0-" + software_version [+ " " + comment] + "\r\n"`.

### 4.2-3
> The 'protoversion' **MUST** be '2.0'.

**CONFORMS** — Hardcoded `"SSH-2.0-"` prefix in `version_tx()`.
`version_rx()` validates via `is_20()` which checks bytes 4–7 are
`"2.0-"`.

### 4.2-4
> If the 'comments' string is included, a 'space' character **MUST**
> separate the 'softwareversion' and 'comments' strings.

**CONFORMS** — `version_tx()` at line 132 inserts a space before the
comment: `memcpy(&sess->trans.tx_packet[sz], " ", 1)`.

### 4.2-5
> The identification **MUST** be terminated by a single Carriage Return
> (CR) and a single Line Feed (LF) character.

**CONFORMS** — `version_tx()` at line 143 appends `"\r\n"`.
`version_rx()` validates via `missing_crlf()` which checks the last two
bytes are `\r\n`.

### 4.2-6
> The null character **MUST NOT** be sent.

**CONFORMS** — `version_tx()` builds the string from `const char *`
strings (NUL-terminated C strings) and explicitly copies only the
non-NUL bytes.  `version_rx()` validates via `has_nulls()` which checks
for embedded NUL using `memchr(buf, 0, buflen)`.

### 4.2-7
> The maximum length of the string is 255 characters, including the
> Carriage Return and Line Feed.

**CONFORMS** — `version_tx()` checks `sz + asz + 2 > 255` before
adding software version and comment (the +2 accounts for CR LF).
`version_rx()` rejects if `received > 255`.

### 4.2-8
> The server **MAY** send other lines of data before sending the version
> string.

**CONFORMS** — `version_rx()` loops calling `rx_line()`, checking each
line with `is_version_line()`.  Non-SSH lines are passed to the
`extra_line_cb` callback if set.

### 4.2-9
> Each line **SHOULD** be terminated by a Carriage Return and Line Feed.

**N/A** — This is about server-sent pre-version lines.  The library as
server only sends its version string, no pre-version lines.

### 4.2-10
> Such lines **MUST NOT** begin with "SSH-".

**CONFORMS** — The library never sends pre-version lines.  When
receiving, `version_rx()` treats any line starting with `"SSH-"` as the
version string, not as a pre-version line, which is correct.

### 4.2-11
> Clients **MUST** be able to process such lines.

**CONFORMS** — `version_rx()` handles pre-version lines via the
`extra_line_cb` callback.  Lines not starting with `"SSH-"` are
processed and the loop continues.

### 4.2-12
> Such lines **MAY** be silently ignored, or **MAY** be displayed to
> the client user.

**CONFORMS** — The `extra_line_cb` callback gives the application the
choice.  If no callback is set, lines are silently ignored.

### 4.2-13
> Both the 'protoversion' and 'softwareversion' strings **MUST** consist
> of printable US-ASCII characters, with the exception of whitespace
> characters and the minus sign (-).

**CONFORMS** — `version_rx()` validates via `has_non_ascii()` which
rejects any byte outside the printable US-ASCII range (0x20–0x7E) in
the version line (before CR-LF).  Our own version string `"DeuceSSH-0.0"`
is pure US-ASCII.

### 4.2-14
> The 'comments' string **SHOULD** contain additional information that
> **MAY** be useful in solving user problems.

**APPLICATION** — The library supports setting `gconf.version_comment`
via the global config.

### 4.2-15
> All packets following the identification string **SHALL** use the
> binary packet protocol.

**CONFORMS** — After `version_exchange()` returns, all communication
uses `send_packet()` / `recv_packet()` which implement the BPP.

---

## Section 5.1: Old Client, New Server

### 5.1-1
> Clients using protocol 2.0 **MUST** be able to identify this ['1.99']
> as identical to '2.0'.

**CONFORMS** — `is_20()` checks for both `"2.0-"` at bytes 4–7 and
`"1.99-"` at bytes 4–8.  The `"1.99"` version indicates an SSH2-capable
server in backward compatibility mode.

### 5.1-2
> When compatibility with old clients is not needed, the server **MAY**
> send its initial key exchange data immediately after sending its
> identification string.

**CONFORMS** — The library sends the version string then immediately
proceeds to KEXINIT.

---

## Section 6: Binary Packet Protocol

### 6-1
> Initially, compression **MUST** be "none".

**CONFORMS** — Compression contexts are NULL after init.  Only `none`
compression is implemented.  No compression is applied before NEWKEYS.

### 6-2
> There **MUST** be at least four bytes of padding.

**CONFORMS** — `send_packet()` at line 303: `if (padding_len < 4)
padding_len += bs`.

### 6-3
> The padding **SHOULD** consist of random bytes.

**CONFORMS** — `send_packet()` at line 319 uses `RAND_bytes()` to fill
the padding.

### 6-4
> Initially, the MAC algorithm **MUST** be "none".

**CONFORMS** — MAC selected pointers are NULL after `transport_init()`.
`tx_mac_size()` and `rx_mac_size()` return 0 when no MAC is selected.

### 6-5
> [The total packet length] **MUST** be a multiple of the cipher block
> size or 8, whichever is larger.  This constraint **MUST** be enforced,
> even when using stream ciphers.

**CONFORMS** — `send_packet()` computes padding to align to the cipher
block size: `padding_len = bs - ((5 + payload_len) % bs)`.
`tx_block_size()` returns `max(enc->blocksize, 8)`, ensuring the minimum
of 8 is enforced even for stream ciphers.

### 6-6
> The minimum size of a packet is 16 (or the cipher block size,
> whichever is larger).

**CONFORMS** — With minimum 4 bytes padding, the smallest packet is
`4 + 1 + 0 + 4 = 9` bytes unpadded, but the block alignment rounds up.
With block size 8 (minimum), the padding becomes at least 7 bytes
(since `5 % 8 = 5`, padding = `8 - 5 = 3`, then `+8 = 11`... wait,
let me check: for a 1-byte payload, `5 + 1 = 6`, `6 % 8 = 6`,
`padding = 8 - 6 = 2`, which is `< 4`, so `+8 = 10`.  Total =
`4 + 1 + 1 + 10 = 16`.  Correct.

### 6-7
> Implementations **SHOULD** decrypt the length after receiving the
> first 8 (or cipher block size) bytes of a packet.

**CONFORMS** — `recv_packet_raw()` reads exactly one block size first
(`gconf.rx(... bs ...)`), decrypts it, then parses `packet_length`
from the first 4 decrypted bytes.

### 6-8
> The insertion of variable amounts of 'random padding' **MAY** help
> thwart attacks at traffic analysis.

**CONFORMS** — Random padding is always inserted.  The library uses
the minimum necessary padding.  Applications wanting additional padding
for traffic analysis resistance can use SSH_MSG_IGNORE.

---

## Section 6.1: Maximum Packet Length

### 6.1-1
> All implementations **MUST** be able to process packets with an
> uncompressed payload length of 32768 bytes or less and total packet
> size of 35000 bytes or less.

**CONFORMS** — Default buffer size is `SSH_BPP_PACKET_SIZE_MIN` =
33280 bytes, which accommodates 32768 bytes of payload plus padding and
headers.  `transport_init()` enforces this as the minimum.

### 6.1-2
> Implementations **SHOULD** support longer packets, where they might
> be needed.

**CONFORMS** — `deuce_ssh_session_init()` accepts a `max_packet_size`
parameter (via `transport_init()`) allowing larger buffers.

### 6.1-3
> Implementations **SHOULD** check that the packet length field is
> reasonable in value in order to avoid denial of service and/or buffer
> overflow attacks.

**CONFORMS** — `recv_packet_raw()` at line 418 checks
`packet_length < 2 || packet_length + 4 > sess->trans.packet_buf_sz`.

---

## Section 6.2: Compression

### 6.2-1
> Compression **MUST** be independent for each direction, and
> implementations **MUST** allow independent choosing of the algorithm
> for each direction.

**CONFORMS** — Separate `comp_c2s_selected` / `comp_s2c_selected`
in transport state.  KEXINIT has separate compression name-lists for
each direction.

### 6.2-2
> It is **RECOMMENDED** that the compression method be the same in both
> directions.

**APPLICATION** — The library registers the same algorithms for both
directions.  The application controls registration.

---

## Section 6.3: Encryption

### 6.3-1
> When encryption is in effect, the packet length, padding length,
> payload, and padding fields of each packet **MUST** be encrypted with
> the given algorithm.

**CONFORMS** — `send_packet()` encrypts the entire packet
(4 + packet_length bytes) via `enc->encrypt()` before transmission.
`recv_packet_raw()` decrypts all received bytes.

### 6.3-2
> The encrypted data in all packets sent in one direction **SHOULD** be
> considered a single data stream.

**CONFORMS** — AES-256-CTR uses a single `EVP_CIPHER_CTX` per
direction, maintaining the counter state across packets.  This makes
the encrypted output a continuous stream.

### 6.3-3
> Initialization vectors **SHOULD** be passed from the end of one packet
> to the beginning of the next packet.

**CONFORMS** — CTR mode implicitly chains the counter.  The
`EVP_CIPHER_CTX` persists across packets, maintaining IV continuity.

### 6.3-4
> All ciphers **SHOULD** use keys with an effective key length of 128
> bits or more.

**CONFORMS** — `aes256-ctr` uses 256-bit keys.  The `none` cipher has
no key (0 bits), but that's by definition.

### 6.3-5
> The ciphers in each direction **MUST** run independently of each
> other.

**CONFORMS** — Separate `enc_c2s_ctx` / `enc_s2c_ctx` contexts in
transport state.  Each direction has its own `EVP_CIPHER_CTX`.

### 6.3-6
> Implementations **MUST** allow the algorithm for each direction to be
> independently selected, if multiple algorithms are allowed by local
> policy.

**CONFORMS** — KEXINIT has separate encryption name-lists for c2s and
s2c.  `negotiate_algo()` is called independently for each direction.

### 6.3-7
> It is **RECOMMENDED** that the same algorithm be used in both
> directions.

**APPLICATION** — The library registers the same algorithms for both
directions by default.

### 6.3-8
> `3des-cbc` [...] However, this algorithm is still **REQUIRED** for
> historical reasons.

**NONCONFORMANT** — `3des-cbc` is not implemented.  This is a conscious
design decision: 3DES has an effective key length of only 112 bits,
uses 64-bit blocks (making it vulnerable to Sweet32 attacks), and modern
SSH implementations are deprecating it (OpenSSH disabled it by default
in 2019).  The REQUIRED status in RFC 4253 predates these developments.
Interoperability with all modern implementations is achieved via
`aes256-ctr`.

### 6.3-9
> The `none` algorithm specifies that no encryption is to be done [...]
> it is **NOT RECOMMENDED**.

**CONFORMS** — The library provides `register_none_enc()` but does not
register it by default.  The application must explicitly opt in.

### 6.3-10
> To implement CBC mode, outer chaining **MUST** be used (i.e., there
> is only one initialization vector).

**N/A** — No CBC ciphers are implemented.

---

## Section 6.4: Data Integrity

### 6.4-1
> Initially, no MAC will be in effect, and its length **MUST** be zero.

**CONFORMS** — See 6-4.

### 6.4-2
> `mac = MAC(key, sequence_number || unencrypted_packet)`

**CONFORMS** — `send_packet()` at lines 328–347 computes MAC over
`sequence_number(uint32) || unencrypted_packet`.  `recv_packet_raw()`
at lines 452–477 verifies using the same formula.

### 6.4-3
> The sequence_number is initialized to zero for the first packet, and
> is incremented after every packet (regardless of whether encryption or
> MAC is in use).  It is never reset, even if keys/algorithms are
> renegotiated later.  It wraps around to zero after every 2^32 packets.

**CONFORMS** — `tx_seq` and `rx_seq` are initialized to 0 in
`transport_init()`.  They are incremented after every packet in
`send_packet()` and `recv_packet_raw()`.  They are `uint32_t` which
naturally wraps at 2^32.  `deuce_ssh_transport_rekey()` does NOT reset
the sequence numbers (only the per-key counters).

### 6.4-4
> The MAC algorithms for each direction **MUST** run independently.

**CONFORMS** — Separate `mac_c2s_selected` / `mac_s2c_selected` and
separate MAC keys (`mac_c2s_ctx` / `mac_s2c_ctx`).

### 6.4-5
> Implementations **MUST** allow choosing the algorithm independently
> for both directions.

**CONFORMS** — Separate MAC name-lists in KEXINIT.  Independent
negotiation via `negotiate_algo()`.

### 6.4-6
> It is **RECOMMENDED** that the same algorithm be used in both
> directions.

**APPLICATION** — Same algorithm set registered for both directions.

### 6.4-7
> The value of 'mac' resulting from the MAC algorithm **MUST** be
> transmitted without encryption as the last part of the packet.

**CONFORMS** — `send_packet()` encrypts `total` bytes (the packet) then
appends the MAC bytes unencrypted (`pos += mac_len` after encryption).
`recv_packet_raw()` reads the MAC separately after reading and
decrypting the packet body.

### 6.4-8
> `hmac-sha1` — **REQUIRED**.

**NONCONFORMANT** — `hmac-sha1` is not implemented.  The library
provides `hmac-sha2-256` instead, which is stronger (SHA-256 vs SHA-1).
All modern SSH implementations support `hmac-sha2-256`.  SHA-1 is
considered cryptographically weak.

### 6.4-9
> `hmac-sha1-96` — **RECOMMENDED**.

**N/A** — Not implemented; `hmac-sha2-256` is provided instead.

---

## Section 6.5: Key Exchange Methods

### 6.5-1
> Two **REQUIRED** key exchange methods have been defined:
> `diffie-hellman-group1-sha1`, `diffie-hellman-group14-sha1`.

**NONCONFORMANT** — Neither `diffie-hellman-group1-sha1` nor
`diffie-hellman-group14-sha1` is implemented.  The library provides
`curve25519-sha256` (RFC 8731) and
`diffie-hellman-group-exchange-sha256` (RFC 4419).

`diffie-hellman-group1-sha1` uses a 1024-bit group (insecure by modern
standards) and SHA-1 (deprecated).
`diffie-hellman-group14-sha1` uses a 2048-bit group but still uses
SHA-1.

Both are widely deprecated.  OpenSSH 8.2+ disabled group1 by default,
and group14-sha1 is being phased out.  All modern implementations
support `curve25519-sha256` which is preferred.

### 6.5-2
> Additional methods **MAY** be defined as specified in [SSH-NUMBERS].

**CONFORMS** — The library supports `curve25519-sha256` (RFC 8731)
and `diffie-hellman-group-exchange-sha256` (RFC 4419), both IANA-
registered.

---

## Section 6.6: Public Key Algorithms

### 6.6-1
> The key type **MUST** always be explicitly known (from algorithm
> negotiation or some other source).

**CONFORMS** — Key type is determined during KEXINIT negotiation
(`key_algo_selected`).  The negotiated algorithm name determines the
key type used.

### 6.6-2
> The certificate part **MAY** be a zero length string, but a public
> key is **REQUIRED**.

**CONFORMS** — The library's key blob formats always include the public
key.  No certificates are used.

### 6.6-3
> Public key/certificate formats that do not explicitly specify a
> signature format identifier **MUST** use the public key/certificate
> format identifier as the signature identifier.

**CONFORMS** — `ssh-ed25519` uses `"ssh-ed25519"` as both the key
format and signature format identifier.  `rsa-sha2-256` uses
`"rsa-sha2-256"` as the signature identifier (per RFC 8332 which
explicitly specifies a different signature format from the key format
`"ssh-rsa"`).

### 6.6-4
> `ssh-dss` — **REQUIRED**.

**NONCONFORMANT** — DSA (`ssh-dss`) is not implemented.  DSA is
considered cryptographically weak (limited to 1024-bit keys in the
original spec), and has been deprecated and disabled by default in
OpenSSH since version 7.0 (2015).  RFC 8332 and subsequent RFCs have
effectively superseded the REQUIRED status of `ssh-dss`.

### 6.6-5
> `ssh-rsa` — **RECOMMENDED**.

**PARTIAL** — The library implements `rsa-sha2-256` (RFC 8332) which
uses the same `"ssh-rsa"` key format but with SHA-256 instead of SHA-1
for signatures.  The original `ssh-rsa` algorithm (SHA-1 signatures) is
not implemented.  RFC 8332 explicitly updates RFC 4253 and the `ssh-rsa`
algorithm's RECOMMENDED status to prefer `rsa-sha2-256`/`rsa-sha2-512`.

---

## Section 7: Key Exchange

### 7-1
> Each side **MAY** guess which algorithm the other side is using, and
> **MAY** send an initial key exchange packet according to the algorithm,
> if appropriate for the algorithm.

**CONFORMS** — The library sets `first_kex_packet_follows = false` in
KEXINIT (line 685), not using the guess optimization.  This is valid
per the RFC (guessing is optional).

### 7-2
> If the guess was wrong, such packets **MUST** be ignored (by the party
> that made the wrong guess).

**CONFORMS** — `deuce_ssh_transport_kexinit()` at lines 817–833 handles
`peer_first_kex_follows`: if the peer's guess was wrong, the next
packet is silently discarded via `recv_packet_raw()`.

### 7-3
> After key exchange with implicit server authentication, the client
> **MUST** wait for a response to its service request message before
> sending any further data.

**CONFORMS** — The library's client-side auth functions are
synchronous — they send a request and wait for a response before
returning.  The auth service request is sent automatically.

---

## Section 7.1: Algorithm Negotiation

### 7.1-1
> Each of the algorithm name-lists **MUST** be a comma-separated list
> of algorithm names.

**CONFORMS** — `build_namelist()` constructs comma-separated lists.
`serialize_namelist_from_str()` serializes them as SSH strings.

### 7.1-2
> Each supported (allowed) algorithm **MUST** be listed in order of
> preference, from most to least.

**CONFORMS** — `build_namelist()` traverses the linked list in
registration order, which is defined as the preference order.

### 7.1-3
> The first algorithm in each name-list **MUST** be the preferred
> (guessed) algorithm.

**CONFORMS** — Follows from 7.1-2.

### 7.1-4
> Each name-list **MUST** contain at least one algorithm name.

**CONFORMS** — The library requires at least one algorithm to be
registered in each category before a session can operate.  If no
algorithm is registered, `build_namelist()` produces an empty string
which would cause negotiation failure.  In practice, calling
`deuce_ssh_session_init()` without registering algorithms is a
programming error.

### 7.1-5
> The 'cookie' **MUST** be a random value generated by the sender.

**CONFORMS** — `deuce_ssh_transport_kexinit()` at line 651 uses
`RAND_bytes()` to generate 16 random bytes for the cookie.

### 7.1-6
> If both sides make the same guess, that algorithm **MUST** be used.

**CONFORMS** — `negotiate_algo()` iterates the client's list and finds
the first match in the server's list.  If both sides list the same
algorithm first, it will be selected.

### 7.1-7
> If no algorithm satisfying all these conditions can be found, the
> connection fails, and both sides **MUST** disconnect.

**CONFORMS** — `deuce_ssh_transport_kexinit()` at lines 800–812 checks
all negotiated values for NULL and sends `SSH_MSG_DISCONNECT` with
reason `SSH_DISCONNECT_KEY_EXCHANGE_FAILED` if any is NULL.

### 7.1-8
> The first algorithm on the client's name-list that satisfies the
> requirements and is also supported by the server **MUST** be chosen.

**CONFORMS** — `negotiate_algo()` iterates the client's list (outer
loop) and for each entry checks the server's list (inner loop).  The
first client algorithm found in the server's list wins.

### 7.1-9
> If there is no such algorithm, both sides **MUST** disconnect.

**CONFORMS** — See 7.1-7.

### 7.1-10
> The chosen encryption algorithm to each direction **MUST** be the
> first algorithm on the client's name-list that is also on the server's
> name-list.

**CONFORMS** — `negotiate_algo()` is called with `client_enc_c2s` /
`server_enc_c2s` (and s2c variants), following the client-preference
rule.

### 7.1-11
> Note that "none" **MUST** be explicitly listed if it is to be
> acceptable.  [Applies to encryption, MAC, and compression.]

**CONFORMS** — The `none` algorithms are separate registration entries.
They only appear in name-lists if explicitly registered by the
application.

### 7.1-12
> The chosen MAC algorithm **MUST** be the first algorithm on the
> client's name-list that is also on the server's name-list.

**CONFORMS** — Same negotiation logic as encryption.

### 7.1-13
> The chosen compression algorithm **MUST** be the first algorithm on
> the client's name-list that is also on the server's name-list.

**CONFORMS** — Same negotiation logic as encryption.

### 7.1-14
> Both parties **MAY** ignore this name-list [languages].

**CONFORMS** — Language name-lists are sent as empty strings.  Received
language lists are parsed but not used.

### 7.1-15
> If there are no language preferences, this name-list **SHOULD** be
> empty.

**CONFORMS** — KEXINIT sends empty language name-lists
(`serialize_namelist_from_str("")`).

### 7.1-16
> Language tags **SHOULD NOT** be present unless they are known to be
> needed by the sending party.

**CONFORMS** — No language tags are sent.

### 7.1-17
> If a guessed packet will be sent, [first_kex_packet_follows] **MUST**
> be TRUE.  If no guessed packet will be sent, this **MUST** be FALSE.

**CONFORMS** — The library sets `first_kex_packet_follows = 0` (FALSE)
and does not send a guessed packet.

### 7.1-18
> If the other party's guess was wrong, and this field was TRUE, the
> next packet **MUST** be silently ignored.

**CONFORMS** — See 7-2.

### 7.1-19
> Both sides **MUST** then act as determined by the negotiated key
> exchange method.  If the guess was right, key exchange **MUST**
> continue using the guessed packet.

**CONFORMS** — After KEXINIT negotiation, `deuce_ssh_transport_kex()`
dispatches to the negotiated KEX handler.  The wrong-guess handling
(7-2) ensures the guessed packet is either consumed or discarded.

### 7.1-20
> Once a party has sent a SSH_MSG_KEXINIT message [...] it **MUST NOT**
> send any messages other than: transport layer generic messages (1 to
> 19), algorithm negotiation messages (20 to 29), specific key exchange
> method messages (30 to 49).

**CONFORMS** — Between KEXINIT and NEWKEYS, the library only sends KEX
method messages (30–49) and NEWKEYS (21).  No higher-layer messages are
sent during this window.

### 7.1-21
> Further SSH_MSG_KEXINIT messages **MUST NOT** be sent [during key
> exchange].

**CONFORMS** — The library does not send additional KEXINIT messages
during key exchange.

### 7.1-22
> Each party **MUST** be prepared to process an arbitrary number of
> messages that **MAY** be in-flight before receiving SSH_MSG_KEXINIT.

**CONFORMS** — During rekey, `send_packet()` blocks application-layer
messages (type >= 50) under `tx_mtx` while `rekey_in_progress` is set.
Transport/KEX messages (types 1–49) pass through.  The rekey path sets
the flag under `tx_mtx` before sending KEXINIT and clears it after
NEWKEYS, then broadcasts `rekey_cnd` to wake blocked senders.
Application-layer in-flight messages from the peer are still processed
normally by the demux thread since `recv_packet_raw()` releases
`rx_mtx` between calls.

NOTE: The rx path (demux thread) sends packets during rekey (KEXINIT,
KEX messages, NEWKEYS, reciprocal CLOSE, WINDOW_ADJUST, global request
replies).  All sends go through `send_packet()` which acquires/releases
`tx_mtx` per call.  No deadlock, but the rx→tx cross-path should be
audited for latency implications.

---

## Section 7.2: Output from Key Exchange

### 7.2-1
> The same hash algorithm **MUST** be used in key derivation.

**CONFORMS** — `derive_key()` uses `sess->trans.kex_selected->hash_name`
which is set by each KEX method (e.g., `"SHA256"` for both
`curve25519-sha256` and `dh-gex-sha256`).

### 7.2-2
> Encryption keys **MUST** be computed as HASH(K || H || letter ||
> session_id).

**CONFORMS** — `derive_key()` in `ssh-trans.c` implements the exact
formula: `HASH(shared_secret || exchange_hash || letter || session_id)`.
Keys A–F are derived per the spec.

### 7.2-3
> Key data **MUST** be taken from the beginning of the hash output.

**CONFORMS** — `derive_key()` copies `min(md_len, needed)` bytes from
the hash output.

### 7.2-4
> If the key length needed is longer than the output of the HASH, the
> key is extended by computing HASH of the concatenation of K and H and
> the entire key so far.

**CONFORMS** — `derive_key()` at lines 890–903 implements the key
extension loop: `HASH(K || H || K1 || K2 || ...)`.

### 7.2-5
> The session identifier [...] is not changed, even if keys are later
> re-exchanged.

**CONFORMS** — `newkeys()` only sets `session_id` if it is NULL
(first key exchange).  `deuce_ssh_transport_rekey()` does not modify
`session_id`.

---

## Section 7.3: Taking Keys Into Use

### 7.3-1
> All messages sent after [SSH_MSG_NEWKEYS] message **MUST** use the new
> keys and algorithms.

**CONFORMS** — `newkeys()` sends SSH_MSG_NEWKEYS, then derives keys and
initializes new cipher/MAC contexts.  All subsequent `send_packet()`
calls use the new contexts.

### 7.3-2
> When this message is received, the new keys and algorithms **MUST** be
> used for receiving.

**CONFORMS** — `newkeys()` receives SSH_MSG_NEWKEYS, then sets up the
receive-direction cipher/MAC contexts.  All subsequent
`recv_packet_raw()` calls use the new contexts.

---

## Section 8: Diffie-Hellman Key Exchange

### 8-1
> Values of 'e' or 'f' that are not in the range [1, p-1] **MUST NOT**
> be sent or accepted by either side.

**CONFORMS** — The DH-GEX implementation (`kex/dh-gex-sha256.c`)
validates received values using `dh_value_valid()`, which checks that
the value is not zero, not negative, and less than p.  This is done
for both `f` (client side, after receiving GEX_REPLY) and `e` (server
side, after receiving GEX_INIT).

For `curve25519-sha256`, the X25519 function has built-in validation.

### 8-2
> The exchange hash **SHOULD** be kept secret.

**CONFORMS** — The exchange hash is stored in `sess->trans.exchange_hash`
and is never exposed to the application except as the session_id (which
is only the first exchange hash and is needed for authentication).

### 8-3
> The signature algorithm **MUST** be applied over H, not the original
> data.

**CONFORMS** — Both KEX handlers sign/verify the exchange hash `H`
directly.  In `curve25519-sha256.c`: `ka->sign(... hash,
SHA256_DIGEST_LEN ...)` and `ka->verify(... hash, SHA256_DIGEST_LEN
...)`.  In `dh-gex-sha256.c`: same pattern.

---

## Section 8.1: diffie-hellman-group1-sha1

### 8.1-1
> This method **MUST** be supported for interoperability.

**NONCONFORMANT** — Not implemented.  See 6.5-1.

---

## Section 8.2: diffie-hellman-group14-sha1

### 8.2-1
> It **MUST** also be supported.

**NONCONFORMANT** — Not implemented.  See 6.5-1.

---

## Section 9: Key Re-Exchange

### 9-1
> When [SSH_MSG_KEXINIT] message is received, a party **MUST** respond
> with its own SSH_MSG_KEXINIT message, except when the received
> SSH_MSG_KEXINIT already was a reply.

**CONFORMS** — `recv_packet()` detects SSH_MSG_KEXINIT (after the
initial key exchange is complete) and automatically handles peer-
initiated rekey: it saves the peer's KEXINIT payload and calls
`deuce_ssh_transport_rekey()`, which sends our KEXINIT, runs KEX, and
exchanges NEWKEYS.  The rekey happens transparently within the
recv_packet call, and the caller sees the next application-layer
message.

### 9-2
> Either party **MAY** initiate the re-exchange, but roles **MUST NOT**
> be changed (i.e., the server remains the server, and the client
> remains the client).

**CONFORMS** — `sess->trans.client` is set once during session setup
and never modified.  `deuce_ssh_transport_rekey()` preserves the
client/server role.

### 9-3
> It is **RECOMMENDED** that the keys be changed after each gigabyte of
> transmitted data or after each hour of connection time, whichever
> comes sooner.

**CONFORMS** — The library automatically rekeys based on three
thresholds, all checked transparently in `recv_packet()`:
- **Packet count**: 2^28 packets per key (RFC 4251 s9.3.2 SHOULD)
- **Byte count**: 1 GiB transferred per key (this recommendation)
- **Elapsed time**: 1 hour per key (this recommendation)

`deuce_ssh_transport_rekey_needed()` checks all three thresholds and
is used by both the auto-rekey logic and available to applications.

### 9-4
> More application data **MAY** be sent after the SSH_MSG_NEWKEYS packet
> has been sent.

**CONFORMS** — After `rekey()` completes, the session immediately
resumes normal operation.

---

## Section 10: Service Request

### 10-1
> If the server rejects the service request, it **SHOULD** send an
> appropriate SSH_MSG_DISCONNECT message and **MUST** disconnect.

**APPLICATION** — Server-side service request handling is the
application's responsibility.

### 10-2
> If the server supports the service (and permits the client to use it),
> it **MUST** respond with [SSH_MSG_SERVICE_ACCEPT].

**APPLICATION** — Same as 10-1.

### 10-3
> Message numbers used by services **SHOULD** be in the area reserved
> for them.

**CONFORMS** — Authentication messages use 50–79, connection messages
use 80–127.

### 10-4
> After a key exchange with implicit server authentication, the client
> **MUST** wait for a response to its service request message before
> sending any further data.

**CONFORMS** — `request_service()` (internal, called automatically by
the auth functions) is synchronous — it sends SSH_MSG_SERVICE_REQUEST
and blocks on `recv_packet()` for SSH_MSG_SERVICE_ACCEPT before
returning.

---

## Section 11.1: Disconnection Message

### 11.1-1
> All implementations **MUST** be able to process this message.

**CONFORMS** — `recv_packet()` handles `SSH_MSG_DISCONNECT` by setting
`terminate = true` and returning `DEUCE_SSH_ERROR_TERMINATED`.

### 11.1-2
> They **SHOULD** be able to send this message.

**CONFORMS** — `deuce_ssh_transport_disconnect()` sends
`SSH_MSG_DISCONNECT` with a reason code and description.

### 11.1-3
> The sender **MUST NOT** send or receive any data after this message.

**CONFORMS** — `deuce_ssh_transport_disconnect()` sets
`sess->terminate = true` after sending.  Subsequent `send_packet()`
calls will fail because the I/O callbacks check `*terminate`.

### 11.1-4
> The recipient **MUST NOT** accept any data after receiving this
> message.

**CONFORMS** — `recv_packet()` sets `sess->terminate = true` on
receiving SSH_MSG_DISCONNECT and returns an error.  Subsequent calls
will fail.

---

## Section 11.2: Ignored Data Message

### 11.2-1
> All implementations **MUST** understand (and ignore) this message at
> any time (after receiving the identification string).

**CONFORMS** — `recv_packet()` handles `SSH_MSG_IGNORE` with
`case SSH_MSG_IGNORE: continue;` — silently discarded and the next
packet is read.

---

## Section 11.3: Debug Message

### 11.3-1
> All implementations **MUST** understand this message, but they are
> allowed to ignore it.

**CONFORMS** — `recv_packet()` handles `SSH_MSG_DEBUG` by parsing the
`always_display` boolean and message string, invoking the `debug_cb`
callback if set, then continuing to the next packet.

### 11.3-2
> If 'always_display' is TRUE, the message **SHOULD** be displayed.

**APPLICATION** — The `debug_cb` callback receives the `always_display`
flag.  Display behavior is the application's responsibility.

### 11.3-3
> Otherwise, it **SHOULD NOT** be displayed unless debugging information
> has been explicitly requested by the user.

**APPLICATION** — Same as 11.3-2.

---

## Section 11.4: Reserved Messages

### 11.4-1
> An implementation **MUST** respond to all unrecognized messages with
> an SSH_MSG_UNIMPLEMENTED message in the order in which the messages
> were received.

**CONFORMS** — `deuce_ssh_transport_send_unimplemented()` sends
SSH_MSG_UNIMPLEMENTED with the rejected packet's sequence number.
Higher-layer code (auth, conn) calls this function when receiving
unexpected message types.  The `last_rx_seq` field in transport state
tracks the sequence number of the last received packet for this purpose.

### 11.4-2
> Such messages **MUST** be otherwise ignored.

**CONFORMS** — After sending UNIMPLEMENTED, the higher-layer code
returns an error to the caller, which can choose how to proceed.

---

## Summary

### Conformance Status

| Status | Count |
|--------|-------|
| CONFORMS | 65 |
| APPLICATION (app responsibility) | 7 |
| N/A | 3 |
| PARTIAL | 1 |
| NONCONFORMANT | 4 |

The remaining PARTIAL is `ssh-rsa` (6.6-5) — the library implements
`rsa-sha2-256` (RFC 8332) rather than the original SHA-1-based
`ssh-rsa` signatures.

### Remaining Issues

1. **`3des-cbc` not implemented** (6.3-8 REQUIRED) — Conscious design
   decision due to Sweet32 vulnerability and 112-bit effective key
   length.  All modern implementations support `aes256-ctr`.

2. **`diffie-hellman-group1-sha1` and `diffie-hellman-group14-sha1` not
   implemented** (6.5-1, 8.1-1, 8.2-1 REQUIRED) — Conscious design
   decision due to SHA-1 deprecation and (for group1) 1024-bit group
   weakness.

3. **`hmac-sha1` not implemented** (6.4-8 REQUIRED) — SHA-1 is
   cryptographically weak.  `hmac-sha2-256` is provided instead.

4. **`ssh-dss` not implemented** (6.6-4 REQUIRED) — DSA with 1024-bit
   keys is considered insecure.  Deprecated by OpenSSH since 7.0.

### Notes on REQUIRED Algorithm Gaps (Items 2–5)

All four REQUIRED algorithms in RFC 4253 (`3des-cbc`, `hmac-sha1`,
`diffie-hellman-group1-sha1`/`group14-sha1`, `ssh-dss`) rely on SHA-1
and/or have known cryptographic weaknesses.  They are deprecated or
disabled by default in all major SSH implementations:

- OpenSSH 7.0 (2015): disabled `ssh-dss` by default
- OpenSSH 7.6 (2017): disabled `hmac-sha1-96` and other weak MACs
- OpenSSH 8.2 (2020): disabled `diffie-hellman-group1-sha1`
- OpenSSH 8.8 (2021): disabled `ssh-rsa` (SHA-1 signatures)

The REQUIRED status in RFC 4253 (2006) predates these deprecations.
DeuceSSH implements the modern algorithm suite that provides
interoperability with all current SSH implementations.
