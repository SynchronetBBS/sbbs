# RFC 4250 Conformance Audit — DeuceSSH

Systematic audit of RFC 4250 (SSH Protocol Assigned Numbers) against
DeuceSSH.  RFC 4250 is primarily an IANA registry document defining
numeric constants and naming conventions.  Most of its requirements
are about the IANA registration process, not about implementations.

Legend:
- **CONFORMS** — library satisfies the requirement
- **N/A** — not applicable (IANA process requirement)

---

## Section 4.1: Message Numbers

### 4.1-1
> Message numbers in the range of 1 to 29, 50 to 59, and 80 to 127
> are allocated via STANDARDS ACTION.

**CONFORMS** — All message numbers used by the library match the
IANA-assigned values:

| Constant | Library Value | RFC Value |
|----------|--------------|-----------|
| SSH_MSG_DISCONNECT | 1 | 1 |
| SSH_MSG_IGNORE | 2 | 2 |
| SSH_MSG_UNIMPLEMENTED | 3 | 3 |
| SSH_MSG_DEBUG | 4 | 4 |
| SSH_MSG_SERVICE_REQUEST | 5 | 5 |
| SSH_MSG_SERVICE_ACCEPT | 6 | 6 |
| SSH_MSG_KEXINIT | 20 | 20 |
| SSH_MSG_NEWKEYS | 21 | 21 |
| SSH_MSG_USERAUTH_REQUEST | 50 | 50 |
| SSH_MSG_USERAUTH_FAILURE | 51 | 51 |
| SSH_MSG_USERAUTH_SUCCESS | 52 | 52 |
| SSH_MSG_USERAUTH_BANNER | 53 | 53 |
| SSH_MSG_GLOBAL_REQUEST | 80 | 80 |
| SSH_MSG_REQUEST_SUCCESS | 81 | 81 |
| SSH_MSG_REQUEST_FAILURE | 82 | 82 |
| SSH_MSG_CHANNEL_OPEN | 90 | 90 |
| SSH_MSG_CHANNEL_OPEN_CONFIRMATION | 91 | 91 |
| SSH_MSG_CHANNEL_OPEN_FAILURE | 92 | 92 |
| SSH_MSG_CHANNEL_WINDOW_ADJUST | 93 | 93 |
| SSH_MSG_CHANNEL_DATA | 94 | 94 |
| SSH_MSG_CHANNEL_EXTENDED_DATA | 95 | 95 |
| SSH_MSG_CHANNEL_EOF | 96 | 96 |
| SSH_MSG_CHANNEL_CLOSE | 97 | 97 |
| SSH_MSG_CHANNEL_REQUEST | 98 | 98 |
| SSH_MSG_CHANNEL_SUCCESS | 99 | 99 |
| SSH_MSG_CHANNEL_FAILURE | 100 | 100 |

### 4.1-2
> Message numbers in the range of 30 to 49 are specific to the key
> exchange method.

**CONFORMS** — KEX-specific messages are defined locally in each KEX
module:
- `SSH_MSG_KEX_ECDH_INIT` (30), `SSH_MSG_KEX_ECDH_REPLY` (31) —
  curve25519-sha256
- `SSH_MSG_KEX_DH_GEX_GROUP` (31), `SSH_MSG_KEX_DH_GEX_INIT` (32),
  `SSH_MSG_KEX_DH_GEX_REPLY` (33), `SSH_MSG_KEX_DH_GEX_REQUEST` (34)
  — dh-gex-sha256

Number reuse between KEX methods is correct per the RFC.

### 4.1-3
> Message numbers in the range of 60 to 79 are specific to the user
> authentication method.

**CONFORMS** — Auth-method-specific messages share number 60:
- `SSH_MSG_USERAUTH_PK_OK` (60) — publickey method
- `SSH_MSG_USERAUTH_PASSWD_CHANGEREQ` (60) — password method
- `SSH_MSG_USERAUTH_INFO_REQUEST` (60) — keyboard-interactive method
- `SSH_MSG_USERAUTH_INFO_RESPONSE` (61) — keyboard-interactive method

Number reuse is correct per the RFC (context-dependent).

### 4.1-4
> Message numbers in the range of 192 to 255 are reserved for local
> extensions (PRIVATE USE).

**CONFORMS** — The library does not use any message numbers in this
range.

---

## Section 4.2: Disconnect Reason Codes

### 4.2-1
> Packets containing SSH_MSG_DISCONNECT **MUST** have Disconnection
> Message 'reason code' values.

**CONFORMS** — `deuce_ssh_transport_disconnect()` always includes a
`uint32_t reason` code in the disconnect message.

### 4.2-2 (Assigned Values)

**CONFORMS** — All 15 disconnect reason codes are defined in
`ssh-trans.h` with correct IANA-assigned values (1–15).

---

## Section 4.3: Channel Connection Failure Reason Codes

### 4.3-1
> Packets containing SSH_MSG_CHANNEL_OPEN_FAILURE **MUST** have Channel
> Connection Failure 'reason code' values.

**N/A** — The library does not currently send
SSH_MSG_CHANNEL_OPEN_FAILURE (this is a known gap — server-side
channel handling is in the application).

---

## Section 4.4: Extended Channel Data Transfer

### 4.4-1
> Packets containing SSH_MSG_CHANNEL_EXTENDED_DATA **MUST** have
> 'data_type_code' values.

**CONFORMS** — The demux thread parses the `data_type_code` field from
CHANNEL_EXTENDED_DATA messages (the type code is at offset 1+4, parsed
as uint32 before the data string).

---

## Section 4.5: Pseudo-Terminal Encoded Terminal Modes

**N/A** — The library does not implement pseudo-terminal requests.
This is an application-level concern handled via CHANNEL_REQUEST
messages that the application constructs directly.

---

## Section 4.6: Names

### 4.6-1
> All names registered by the IANA **MUST** be printable US-ASCII
> strings, and **MUST NOT** contain the characters at-sign ("@"),
> comma (","), whitespace, control characters (ASCII codes 32 or less),
> or the ASCII code 127 (DEL).  Names are case-sensitive, and **MUST
> NOT** be longer than 64 characters.

**CONFORMS** — Covered in detail in audit-4251.md (sections 6-1
through 6-4).  All registered algorithm names are validated:
- Received names: character validation in `deuce_ssh_parse_namelist()`
  and 64-character limit in KEXINIT parsing
- Registered names: length and emptiness checks in all
  `deuce_ssh_transport_register_*()` functions
- All built-in names are IANA-compliant US-ASCII strings under 64
  characters

### 4.6-2
> Names with the at-sign ("@") [...] **MUST** be printable US-ASCII
> strings [...] **MUST** have only a single at-sign in them.

**N/A** — The library does not use vendor extension names.  Vendor
names from peers are parsed correctly but not negotiated (no matching
registered algorithm).

---

## Summary

| Status | Count |
|--------|-------|
| CONFORMS | 7 |
| N/A | 3 |

No issues found.  All IANA-assigned numeric constants match their
registered values.  Naming conventions are enforced per audit-4251.md.
