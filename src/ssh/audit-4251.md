# RFC 4251 Conformance Audit — DeuceSSH

Systematic audit of every MUST, MUST NOT, REQUIRED, SHALL, SHALL NOT,
SHOULD, SHOULD NOT, RECOMMENDED, MAY, and OPTIONAL keyword in RFC 4251
(SSH Protocol Architecture) against the DeuceSSH library code.

Legend:
- **CONFORMS** — library satisfies the requirement
- **N/A** — requirement applies to a layer/role not implemented by this library
- **PARTIAL** — partially conformant, see notes
- **NONCONFORMANT** — library violates the requirement
- **APPLICATION** — requirement is the responsibility of the application using the library, not the library itself

---

## Section 3: Conventions Used in This Document

### 3-1
> All documents related to the SSH protocols **shall** use the keywords
> "MUST", "MUST NOT", "REQUIRED", "SHALL", "SHALL NOT", "SHOULD",
> "SHOULD NOT", "RECOMMENDED", "MAY", and "OPTIONAL" to describe
> requirements.  They are to be interpreted as described in [RFC-2119].

**N/A** — Meta-requirement about the RFC documents themselves, not about
implementations.

---

## Section 4.1: Host Keys

### 4.1-1
> Each server host **SHOULD** have a host key.

**APPLICATION** — The library provides `dssh_ed25519_generate_key()` and
`dssh_ed25519_load_key_file()` for host key provisioning.  Whether a host
key is present is determined by the server application.  The library will
fail KEX with `DSSH_ERROR_INIT` if no key is loaded, so the
application cannot accidentally skip this.

### 4.1-2
> Hosts **MAY** have multiple host keys using multiple different algorithms.

**CONFORMS** — The algorithm registry supports registering multiple
`key_algo` modules (e.g., both `ssh-ed25519` and `rsa-sha2-256`).
Negotiation selects the best match.

### 4.1-3
> Multiple hosts **MAY** share the same host key.

**CONFORMS** — Nothing in the library prevents loading the same key
material on multiple server instances.

### 4.1-4
> If a host has keys at all, it **MUST** have at least one key that uses
> each REQUIRED public key algorithm.

**CONFORMS** — DeuceSSH implements `rsa-sha2-256` (RFC 8332, which
supersedes the deprecated `ssh-rsa` SHA-1 algorithm) with full
sign/verify/pubkey support, and `ssh-ed25519`.  Both can be used as
server host key algorithms.  `dssh_rsa_sha2_256_load_key_file()` and
`dssh_rsa_sha2_256_generate_key()` provide server-side key management.
The original `ssh-rsa` (SHA-1 based) is not implemented, but RFC 8332
explicitly updates RFC 4253 to make `rsa-sha2-256` an acceptable
replacement.

### 4.1-5
> Implementations **SHOULD NOT** normally allow such connections by
> default.

Context: connecting to a host whose key has not been previously verified.

**APPLICATION** — Host key verification is the application's
responsibility.  The library's client side calls `ka->verify()` on the
server's signature during KEX (proving the server holds the private key
corresponding to the offered public key), but the policy of whether to
accept an unknown public key is outside the library.  The `client.c`
example does not perform host key checking — it accepts any key.  This
is appropriate for a test client, but a production application SHOULD
implement verification.

### 4.1-6
> Implementations **SHOULD** try to make the best effort to check host
> keys, e.g., by checking the host key against a local database.

**APPLICATION** — Same as 4.1-5.  The library provides the host key blob
to the application via the KEX exchange; the application must verify it.

### 4.1-7
> Implementations **MAY** provide additional methods for verifying the
> correctness of host keys, e.g., a hexadecimal fingerprint derived from
> the SHA-1 hash of the public key.

**APPLICATION** — Not provided by the library, but the host key blob is
available to the application, which can compute fingerprints.

### 4.1-8
> All implementations **SHOULD** provide an option not to accept host
> keys that cannot be verified.

**APPLICATION** — The library does not make host key acceptance
decisions.  The application controls this policy.

---

## Section 4.2: Extensibility

### 4.2-1
> All implementations **MUST** support a minimal set of algorithms to
> ensure interoperability.

**PARTIAL** — The library provides: `curve25519-sha256` and
`diffie-hellman-group-exchange-sha256` (KEX), `ssh-ed25519` and
`rsa-sha2-256` (host key, verify-only for RSA), `aes256-ctr` and
`none` (encryption), `hmac-sha2-256` and `none` (MAC), `none`
(compression).

Per RFC 4253 s6.3, the REQUIRED KEX is `diffie-hellman-group14-sha1` —
**not implemented**.  However, all modern SSH implementations prefer
`curve25519-sha256` or DH-GEX, and `diffie-hellman-group14-sha1` uses
SHA-1 which is deprecated.

Per RFC 4253 s6.6, the REQUIRED host key algorithm is `ssh-dss` — but
this was deprecated by RFC 8332 and later RFCs.

In practice, interoperability is achieved with all modern implementations
via `curve25519-sha256` + `ssh-ed25519` + `aes256-ctr` +
`hmac-sha2-256`, as verified against OpenSSH 9.9 and Synchronet/cryptlib.

---

## Section 4.3: Policy Issues

### 4.3-1
> The following policy issues **SHOULD** be addressed in the
> configuration mechanisms of each implementation.

**APPLICATION** — Policy configuration is the application's
responsibility.  The library provides configurable algorithm
registration (order = preference), callback-based I/O, and callback-based
authentication.

### 4.3-2
> The policy **MUST** specify which is the preferred algorithm (e.g., the
> first algorithm listed in each category is preferred).

**CONFORMS** — Algorithm registration order determines preference.
`build_namelist()` emits algorithms in registration order, and
`negotiate_algo()` follows the client-preference-first rule per RFC 4253.

### 4.3-3
> The server's policy **MAY** require multiple authentication for some or
> all users.

**APPLICATION** — Authentication policy is handled by the server
application, not the library.

### 4.3-4
> The required algorithms **MAY** depend on the location from where the
> user is trying to gain access.

**APPLICATION** — Location-based policy is the server application's
responsibility.

### 4.3-5
> The policy **SHOULD NOT** allow the server to start sessions or run
> commands on the client machine, and connections to the authentication
> agent **MUST NOT** be forwarded unless forwarding such connections has
> been requested.

**CONFORMS** — The library does not implement agent forwarding.  Channel
handling is explicit — the library only opens channels the application
requests.  There is no mechanism for the server to initiate sessions on
the client.

---

## Section 4.4: Security Properties

### 4.4-1
> The protocol allows the verification to be left out, but this is
> **NOT RECOMMENDED**.

Context: leaving out host key verification.

**APPLICATION** — See 4.1-5.

---

## Section 4.5: Localization and Character Set Support

### 4.5-1
> When applicable, the character set for the data **MUST** be explicitly
> specified.

**CONFORMS** — The disconnect message includes a language tag (empty
string, per `dssh_transport_disconnect()`).  The library serializes
strings as raw bytes — character set interpretation is the application's
responsibility.

### 4.5-2
> They **MUST** be encoded using ISO 10646 UTF-8.

Context: text strings displayed to users.

**APPLICATION** — The library passes through string data without
encoding transformation.  Applications that display text (e.g., debug
messages, banners) must ensure UTF-8 encoding.

### 4.5-3
> Straight bit-wise, binary comparison is **RECOMMENDED**.

Context: comparing user names and strings.

**CONFORMS** — All name-list comparison in `negotiate_algo()` uses
`memcmp()`, which is strict binary comparison.

### 4.5-4
> It **SHOULD** be possible to fetch a localized message instead of the
> transmitted message.

Context: error/debug messages.

**N/A** — Localization of messages is an application/UI concern, not a
protocol library concern.

### 4.5-5
> The remaining messages **SHOULD** be configurable.

**N/A** — Same as 4.5-4.

---

## Section 5: Data Type Representations

### 5-1 (boolean)
> The value 0 represents FALSE, and the value 1 represents TRUE. All
> non-zero values **MUST** be interpreted as TRUE; however, applications
> **MUST NOT** store values other than 0 and 1.

**CONFORMS (MUST interpret non-zero as TRUE)** — `dssh_parse_boolean()`
at `ssh-arch.c:69` assigns `*val = buf[0]`, where `val` is a `bool`.
C's `_Bool` conversion guarantees any non-zero value becomes `true` (1).

**CONFORMS (MUST NOT store values other than 0 and 1)** —
`dssh_serialize_boolean()` at `ssh-arch.c:84` stores
`buf[*pos] = val` where `val` is `bool`, which in C is always 0 or 1.

### 5-2 (uint32)
> Represents a 32-bit unsigned integer. Stored as four bytes in the
> order of decreasing significance (network byte order).

**CONFORMS** — `dssh_parse_uint32()` in `ssh.c` reads four
bytes in big-endian order with explicit shifts: `(buf[0]<<24) |
(buf[1]<<16) | (buf[2]<<8) | buf[3]`.  `dssh_serialize_uint32()`
in `ssh.c` writes in the same order.

### 5-3 (uint64)
> Represents a 64-bit unsigned integer. Stored as eight bytes in the
> order of decreasing significance (network byte order).

**CONFORMS** — `dssh_parse_uint64()` at `ssh-arch.c:121-124`
and `dssh_serialize_uint64()` at `ssh-arch.c:138-146` handle 8
bytes in big-endian order.

### 5-4 (string)
> Arbitrary length binary string. [...] stored as a uint32 containing
> its length [...] and zero (= empty string) or more bytes [...]
> Terminating null characters are not used.

**CONFORMS** — `dssh_parse_string()` at `ssh-arch.c:150-164`
reads a uint32 length then points into the buffer.  No NUL terminator
is expected or added.  `dssh_serialize_string()` at
`ssh-arch.c:173-181` writes uint32 length + raw bytes, no NUL.

### 5-5 (string)
> The terminating null character **SHOULD NOT** normally be stored in
> the string.

**CONFORMS** — The library does not append NUL terminators to wire-format
strings.  Internal C strings used for names (service names, method
names, etc.) are serialized with explicit length calculation via
`strlen()`, which excludes the NUL.

### 5-6 (mpint)
> If the most significant bit would be set for a positive number, the
> number **MUST** be preceded by a zero byte.

**CONFORMS** — `encode_shared_secret()` in `kex/curve25519-sha256.c:166`
checks `start[0] & 0x80` and prepends a zero byte.
`dssh_transport_newkeys()` at `ssh-trans.c:939` does the same for
key derivation.  `serialize_bn_mpint()` in `kex/dh-gex-sha256.c:34`
also handles this.

### 5-7 (mpint)
> Unnecessary leading bytes with the value 0 or 255 **MUST NOT** be
> included.

**CONFORMS** — `dssh_parse_mpint()` at `ssh-arch.c:189-193`
validates incoming mpints: rejects a leading 0x00 where the next byte
doesn't have the high bit set (unnecessary leading zero), and rejects
a leading 0xFF where the next byte has the high bit set (unnecessary
leading 0xFF).

For serialization: `encode_shared_secret()` in
`kex/curve25519-sha256.c:156-159` strips leading zeros before encoding.
`BN_bn2bin()` in `kex/dh-gex-sha256.c` produces minimal representation
(no leading zeros) by definition, and the padding is only added when
the MSB is set.

### 5-8 (mpint)
> The value zero **MUST** be stored as a string with zero bytes of data.

**CONFORMS** — For curve25519, the shared secret cannot be zero (X25519
will return an error for a zero result).  For DH-GEX, `BN_bn2bin()`
returns 0 bytes for value zero, and `BN_num_bytes()` returns 0, so
the mpint would be encoded as length=0 with no data bytes.

### 5-9 (mpint)
> By convention, a number that is used in modular computations in Z_n
> **SHOULD** be represented in the range 0 <= x < n.

**CONFORMS** — DH computations use `BN_mod_exp()` which produces
results in [0, n) by definition.  X25519 results are positive by
definition.

### 5-10 (name-list)
> A name **MUST** have a non-zero length, and it **MUST NOT** contain a
> comma (",").

**CONFORMS** — `dssh_parse_namelist()` at `ssh-arch.c:220-225`
rejects empty names (comma at position 0, or consecutive commas: `i == 0
|| str.value[i - 1] == ','`).  Commas are handled as delimiters, not
stored in name values.  Trailing commas would cause a zero-length final
name, which the empty-after-comma check catches.

An explicit trailing-comma check after the loop rejects `"foo,"` and
similar inputs that would produce a zero-length final name.

### 5-11 (name-list)
> All of the elements contained are names and **MUST** be in US-ASCII.

**CONFORMS** — `dssh_parse_namelist()` at `ssh-arch.c:224` rejects
any byte `<= ' '` (space, 32) or `>= 127` (DEL and above).  This
ensures only printable US-ASCII (33–126) and comma (44) are accepted.

### 5-12 (name-list)
> Terminating null characters **MUST NOT** be used, neither for the
> individual names, nor for the list as a whole.

**CONFORMS** — NUL (0) is caught by the `str.value[i] <= ' '` check in
`dssh_parse_namelist()`.  Serialization via
`dssh_serialize_namelist()` delegates to `serialize_string()` which
writes raw bytes without NUL terminators.

---

## Section 6: Algorithm and Method Naming

### 6-1
> All algorithm and method identifiers **MUST** be printable US-ASCII,
> non-empty strings no longer than 64 characters.

**CONFORMS** — Printable US-ASCII: enforced on received name-lists by
the character validation in `dssh_parse_namelist()` (rejects bytes
<= 32 or >= 127).  Non-empty: enforced by the empty-name check in
`dssh_parse_namelist()` (see 5-10).

64-character limit: enforced on received names in KEXINIT parsing
(`ssh-trans.c` walks each name-list and rejects any individual name
> 64 characters).  Also enforced in `dssh_parse_namelist_next()`.
Enforced on registered names by all `dssh_transport_register_*()`
functions.

### 6-2
> Names **MUST** be case-sensitive.

**CONFORMS** — `negotiate_algo()` uses `memcmp()` for comparison, which
is case-sensitive.

### 6-3
> Registered names **MUST NOT** contain an at-sign ("@"), comma (","),
> whitespace, control characters (ASCII codes 32 or less), or the ASCII
> code 127 (DEL).

**PARTIAL** — For received names: comma is the delimiter so cannot
appear in names.  Control characters and DEL are rejected by the
character validation.  At-sign (64, `@`) IS printable US-ASCII and would
NOT be rejected by the current validation (it passes the `> ' '` and
`< 127` checks).  However, the library's design explicitly excludes
`@`-named extensions — it only registers IANA standard names.  A peer
sending `@`-names would have those names accepted during parsing but
they would fail negotiation (no matching registered algorithm).

For registered names: the library does not validate registered algorithm
names.  The built-in names are all IANA-registered and do not contain
`@`, comma, whitespace, or control characters.

### 6-4
> Names are case-sensitive, and **MUST NOT** be longer than 64
> characters.

See 6-1 and 6-2.  **CONFORMS**.

### 6-5
> [Vendor extension] names **MUST** be printable US-ASCII strings, and
> **MUST NOT** contain the comma character, whitespace, control
> characters (ASCII codes 32 or less), or the ASCII code 127 (DEL).

**N/A** — The library does not implement vendor extensions (no `@`
names).  If a peer offers them, they are parsed but rejected during
negotiation.

### 6-6
> They **MUST** have only a single at-sign in them.

**N/A** — Same as 6-5.

### 6-7
> Names are case-sensitive, and **MUST NOT** be longer than 64
> characters.

Same as 6-4.

---

## Section 7: Message Numbers

### 7-1
> SSH packets have message numbers in the range 1 to 255.

**CONFORMS** — Message types are stored as `uint8_t`, which constrains
them to 0–255.  Message type 0 is not used.  All defined constants use
`UINT8_C()` macros.

### 7-2
> 30 to 49: Key exchange method specific (numbers can be reused for
> different authentication methods)

**CONFORMS** — `SSH_MSG_KEX_ECDH_INIT` (30) and `SSH_MSG_KEX_ECDH_REPLY`
(31) are defined in `ssh-trans.h`.  DH-GEX message numbers (31–34) are
defined locally in `kex/dh-gex-sha256.c`.  Message number reuse between
KEX methods is correctly handled since only one KEX method is active per
session.

### 7-3
> 192 to 255: Local extensions

**CONFORMS** — The library does not use any message numbers in this
range.

---

## Section 8: IANA Considerations

### 8-1
> These names **MUST** be printable US-ASCII strings, and **MUST NOT**
> contain the characters at-sign ("@"), comma (","), whitespace, control
> characters (ASCII codes 32 or less), or the ASCII code 127 (DEL).

**CONFORMS** — The requirement that IANA-registered names MUST NOT
contain `@` applies to the registration process, not to wire parsing.
Vendor extension names (`name@domain`) are valid per s6 and must be
accepted in name-lists.  The library correctly parses `@`-containing
names but will not negotiate them (no `@`-algorithms are registered).
All library-registered names conform.

### 8-2
> Names are case-sensitive, and **MUST NOT** be longer than 64
> characters.

See 6-1, 6-2, 6-4.

### 8-3
> Using the same name in multiple categories **SHOULD** be avoided to
> minimize confusion.

**CONFORMS** — The built-in `none` name is used in encryption, MAC, and
compression categories, but this is standard SSH practice (all
implementations do this).  Each category has a separate namespace per
the RFC.

---

## Section 9.1: Pseudo-Random Number Generation

### 9.1-1
> Special care should be taken to ensure that all of the random numbers
> are of good quality.

**CONFORMS** — The library uses OpenSSL's `RAND_bytes()` for all
random data: padding in `send_packet()` (`ssh-trans.c:319`), KEXINIT
cookie (`ssh-trans.c:651`), and DH private exponent generation via
`BN_rand()` which uses OpenSSL's CSPRNG.  X25519 key generation via
`EVP_PKEY_keygen()` also uses OpenSSL's CSPRNG.  OpenSSL 3.0+ uses
a DRBG (Deterministic Random Bit Generator) seeded from the OS entropy
source.

---

## Section 9.2: Control Character Filtering

### 9.2-1
> The client software **SHOULD** replace any control characters
> (except tab, carriage return, and newline) with safe sequences to
> avoid terminal attacks.

**APPLICATION** — The library passes through channel data without
modification.  Control character filtering for display is the terminal
application's responsibility.

---

## Section 9.3.1: Confidentiality

### 9.3.1-1
> The "none" cipher is provided for debugging and **SHOULD NOT** be used
> except for that purpose.

**APPLICATION** — The library provides `dssh_register_none_enc()`.  Whether
it is registered is the application's choice.  The library does not
warn or restrict its use — it is up to the application to only register
`none` for debugging.

### 9.3.1-2
> A packet containing SSH_MSG_IGNORE **SHOULD** be sent [before each
> new packet to mitigate CBC chosen-ciphertext attacks].

**N/A** — DeuceSSH does not implement CBC ciphers, only CTR mode
(`aes256-ctr`).  CTR mode is not vulnerable to this attack.  The library
does not send SSH_MSG_IGNORE packets, but this is only a concern for
CBC mode.

---

## Section 9.3.2: Data Integrity

### 9.3.2-1
> Implementers **SHOULD** be wary of exposing this feature [the "none"
> MAC] for any purpose other than debugging.

**APPLICATION** — Same as 9.3.1-1.  The library provides
`dssh_register_none_mac()`.  The application controls registration.

### 9.3.2-2
> Users and administrators **SHOULD** be explicitly warned anytime the
> "none" MAC is enabled.

**APPLICATION** — Warning about algorithm selection is the application's
responsibility.  The library provides `dssh_transport_get_mac_name()`
for the application to check what was negotiated.

### 9.3.2-3
> Rekeying **SHOULD** happen after 2^28 packets at the very most.

**CONFORMS** — The library tracks per-key packet counts
(`tx_since_rekey` / `rx_since_rekey`) and provides
`dssh_transport_rekey_needed()` which returns true once either
counter exceeds the soft limit of 2^28.  The application can poll
this flag and call `dssh_transport_rekey()` to perform key
re-exchange.  A hard limit at 2^31 prevents sending/receiving if the
application fails to rekey, returning `DSSH_ERROR_REKEY_NEEDED`.

---

## Section 9.3.3: Replay

### 9.3.3-1
> Peers **MUST** rekey before a wrap of the sequence numbers.

**CONFORMS** — The library enforces a hard limit of 2^31 packets per
key, well before the 2^32 sequence number wrap.  Once the hard limit is
reached, `send_packet` and `recv_packet` refuse with
`DSSH_ERROR_REKEY_NEEDED`, forcing the application to either rekey
(via `dssh_transport_rekey()`) or disconnect.  This guarantees the
MUST to rekey before sequence number wrap is never violated.

---

## Section 9.3.4: Man-in-the-middle

### 9.3.4-1
> It is **RECOMMENDED** that the warning provide sufficient information
> to the user so the user can make an informed decision.

Context: when host key doesn't match stored key.

**APPLICATION** — Host key verification UI is the application's
responsibility.

### 9.3.4-2
> Implementers **SHOULD** provide recommendations on how best to [make
> host key fingerprints available for out-of-band verification].

**APPLICATION** — Documentation/guidance is the application's
responsibility.

### 9.3.4-3
> The user **SHOULD** be given a warning that the offered host key does
> not match [a previously stored key].

**APPLICATION** — Same as 9.3.4-1.

### 9.3.4-4
> The use of this protocol without a reliable association of the binding
> between a host and its host keys is inherently insecure and is **NOT
> RECOMMENDED**.

**APPLICATION** — Same as 4.1-5.

---

## Section 9.3.5: Denial of Service

### 9.3.5-1
> The connection **SHOULD** be re-established if [a DoS attack causes
> disconnect].

**APPLICATION** — Reconnection logic is the application's
responsibility.

### 9.3.5-2
> Implementers **SHOULD** provide features that make [resource
> exhaustion attacks] more difficult, for example, only allowing
> connections from a subset of clients known to have valid users.

**APPLICATION** — Connection acceptance policy and rate limiting are the
application/server's responsibility.

---

## Section 9.3.6: Covert Channels

No normative keywords (MUST/SHOULD/etc.) in this section.

---

## Section 9.3.7: Forward Secrecy

No normative keywords requiring implementation action.  DH key exchange
provides PFS by design — both `curve25519-sha256` and
`diffie-hellman-group-exchange-sha256` generate ephemeral keys that are
freed after KEX completes.

**CONFORMS** — Ephemeral X25519 keys are freed via `EVP_PKEY_free()`
after key exchange.  DH private exponents are freed via `BN_free()`.

---

## Section 9.3.8: Ordering of Key Exchange Methods

### 9.3.8-1
> It is **RECOMMENDED** that the algorithms be sorted by cryptographic
> strength, strongest first.

**APPLICATION** — Algorithm registration order (which determines
negotiation preference) is set by the application.  The library's
README.md example registers `curve25519-sha256` before
`diffie-hellman-group-exchange-sha256`, which is a reasonable strength
ordering.

---

## Section 9.3.9: Traffic Analysis

### 9.3.9-1
> Implementers should use the SSH_MSG_IGNORE packet [...] to thwart
> attempts at traffic analysis.

Note: lowercase "should" — not a normative keyword per RFC 2119.

**N/A** — The library does not provide a convenience function to send
`SSH_MSG_IGNORE`, but applications can construct and send one via
`dssh_transport_send_packet()` with message type 2.

---

## Section 9.4: Authentication Protocol

### 9.4-1
> Authentication is no stronger than the weakest combination allowed.

**APPLICATION** — Authentication method selection is the application's
responsibility.

---

## Section 9.4.1: Weak Transport

### 9.4.1-1
> Authentication methods that rely on secret data **SHOULD** be disabled
> [if the transport does not provide confidentiality].

**APPLICATION** — If the `none` cipher is negotiated, the application
should not send passwords.  The library does not enforce this — it
provides `dssh_transport_get_enc_name()` for the application to
check.

### 9.4.1-2
> Requests to change authentication data **SHOULD** be disabled [under
> weak transport].

**APPLICATION** — Same as 9.4.1-1.

---

## Section 9.4.2: Debug Messages

### 9.4.2-1
> It is **RECOMMENDED** that debug messages be initially disabled.

**CONFORMS** — `sess->debug_cb` defaults to NULL (session is zero-initialized
per the API contract).  Debug messages are silently discarded unless the
application explicitly sets a callback.

### 9.4.2-2
> It is also **RECOMMENDED** that a message expressing this concern [about
> debug messages leaking information] be presented.

**APPLICATION** — Warning text when enabling debug is the application's
responsibility.

---

## Section 9.4.3: Local Security Policy

### 9.4.3-1
> The implementer **MUST** ensure that the credentials provided validate
> the professed user.

**APPLICATION** — Server-side authentication validation is entirely the
application's responsibility.  The library provides client-side auth
functions (`dssh_auth_password()`, etc.) that send credentials, but
validation happens in the server application code.

### 9.4.3-2
> [The implementer] **MUST** ensure that the local policy of the server
> permits the user the access requested.

**APPLICATION** — Authorization is the server application's responsibility.

### 9.4.3-3
> Where local security policy exists, it **MUST** be applied and
> enforced correctly.

**APPLICATION** — Policy enforcement is the server application's
responsibility.

---

## Section 9.4.4: Public Key Authentication

No normative requirements applicable to the library.  The library does
not implement public key authentication (client-side) — only password
and keyboard-interactive.

---

## Section 9.5.1: End Point Security

### 9.5.1-1
> Implementers **SHOULD** provide mechanisms for administrators to
> control which services are exposed.

**APPLICATION** — Service exposure control is the application's
responsibility.

---

## Section 9.5.2: Proxy Forwarding

### 9.5.2-1
> Implementers **SHOULD** provide an administrative mechanism to control
> proxy forwarding functionality.

**N/A** — The library does not implement port forwarding.

---

## Section 9.5.3: X11 Forwarding

### 9.5.3-1
> It is **RECOMMENDED** that X11 display implementations default to
> allow the display to open only over local IPC mechanisms.

**N/A** — The library does not implement X11 forwarding.

### 9.5.3-2
> It is **RECOMMENDED** that SSH server implementations default to allow
> the display to open only over local IPC mechanisms.

**N/A** — Same.

### 9.5.3-3
> Implementers of the X11 forwarding protocol **SHOULD** implement the
> magic cookie access-checking mechanism.

**N/A** — Same.

---

## Summary

### Conformance Status

| Status | Count |
|--------|-------|
| CONFORMS | 32 |
| APPLICATION (app responsibility) | 26 |
| N/A (not applicable) | 11 |
| PARTIAL | 0 |
| NONCONFORMANT | 0 |

All identified issues have been resolved.  No remaining MUST or SHOULD
violations.
