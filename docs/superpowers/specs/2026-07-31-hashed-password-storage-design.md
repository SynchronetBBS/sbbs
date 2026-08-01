# Hashed password storage

**Date:** 2026-07-31
**Revised:** 2026-08-01, following review by Deuce
**Status:** Design, not yet approved for implementation

The 2026-08-01 revision restated the threat model as two testable properties,
recorded the decision that the change is mandatory rather than optional,
corrected two factual errors in the first draft (browser support for HTTP
authentication, and the claim that HTTP Basic could be moved onto session
tokens), completed the audit of password sinks, and promoted the crypto-library
choice and the IMAP transport work to prerequisites.

## Problem

Synchronet stores every user's password in cleartext, as field 47 of each
record in `data/user/user.tab` (parsed at `userdat.c:545`, written at
`userdat.c:822`).

## Threat model

The adversary is **possession of the user database**, by whatever route: a copy
taken by anyone with read access to `data/`, a backup, one of the rotated
copies the BBS makes itself, a co-sysop, a resold drive, a breach. The
adversary is the file, not a person with a job title. The asset being protected
is not the user's access to *this* BBS — it is the user's password *as reused
on other systems*. BBS users reuse passwords; a leaked user database burns
accounts elsewhere.

The goal reduces to two properties of the stored data. They are the acceptance
test for every proposal in this document, and nothing below is justified by
assertion where it can be justified by these:

- **P1 — indistinguishability.** Two users with the same password must not
  produce the same stored record. A file that answers "which of these accounts
  share a password" is a map of which accounts to attack together, and it leaks
  even when nothing is cracked. This requires a per-user salt.
- **P2 — non-correlatability.** Given the file and a candidate plaintext, it
  must not be cheap to confirm whether that plaintext is the user's password.
  This is what stops a copy of the database from being turned into working
  credentials elsewhere. This requires a deliberately slow KDF.

Explicitly **out** of scope, and the design must not claim otherwise:

- A sysop impersonating a user on this BBS. They can simply set a new password.
- A **malicious** sysop generally. Anyone who can instrument the running system
  captures passwords as they arrive, and no storage design prevents it. What
  this design addresses is the *careless* handling of a database that a
  malicious party later obtains — which is how these files actually escape.
- The live authentication path. The server necessarily sees the password at
  login. P1 and P2 are properties of data at rest.

### Transport is a separate problem

Plaintext authentication over TLS leans on TLS, and TLS is broadly
MITM-inspected in corporate environments. That objection is real, but it is not
an argument for challenge-response: an intercepted CRAM-MD5 challenge and
response is an unsalted MD5 HMAC pair over attacker-chosen input, which is
offline-crackable at GPU speed. A MITM proxy recovers weak passwords from a
CRAM-MD5 exchange much as it reads them from a plaintext one.

The mechanisms that genuinely resist an inspecting proxy are SCRAM with channel
binding, and client certificates. Neither changes P1 or P2, and neither is
blocked by this design.

## Why encryption was rejected

Encryption at rest protects against an attacker who obtains the data but not
the key. Here the key must be readable by every server process on every host
sharing the installation, which means it lives in the same directory tree, on
the same disk, in the same backup. Whatever takes the database takes the key
with it.

Encryption also fails P1 outright: under any deterministic scheme, two users
with the same password encrypt to the same ciphertext, so the file still
answers "who shares a password" without decrypting anything.

Only a slow, salted, one-way hash satisfies both properties.

## Mandatory, not optional

Hashed storage is **not** a sysop-selectable mode. There is no configuration
item that turns it off, and no supported path back to cleartext.

The argument is about who bears the cost and who makes the choice. The party
who benefits from P1 and P2 is the user with a reused password, and that user
cannot tell a BBS that hashes from one that does not — there is no banner, no
protocol signal, nothing observable at login. Making it optional therefore
assigns the decision to the one party that bears none of the consequence.

Two supporting arguments:

- An optional mode has to remain reachable, so the cleartext path stays in the
  codebase, stays wired into every server, and stays something that has to be
  tested and kept working indefinitely. The deprecations below buy nothing if
  the code they remove has to be retained behind a flag.
- P1 and P2 are properties of a **file**, and user databases circulate between
  systems and across time — backups, host migrations, sysop-to-sysop transfers.
  A per-system opt-out is a per-system hole that travels with the copy.

The consequences are real and belong in the same breath as the decision:

- **The per-system switch disappears.** The digest mechanisms are removed
  outright rather than gated, which simplifies the work but removes the escape
  hatch a cautious sysop would reach for.
- **IMAP on port 143 breaks at upgrade** for CRAM-MD5 clients. That promotes
  STARTTLS or IMAPS from an open question to a hard prerequisite: it must ship
  before the deprecation, not alongside it.
- **There is no rollback.** A sysop who upgrades and finds a mail client broken
  cannot revert, because the cleartext is gone by then. That is precisely why
  the transport prerequisite is not negotiable, and why migration has to be
  boring.

## Why the digest authentication schemes must go

Any challenge-response scheme in which the *server* verifies an HMAC or digest
keyed by the password requires the server to hold something password-equivalent.
No storage format changes this: the stored material may be a *substitute* for
the password rather than the password itself (see "Per-mechanism verifiers"
below), but it is always sufficient to authenticate, and always cheap to attack
offline. The affected mechanisms:

| Mechanism | Site | Fate |
|---|---|---|
| SMTP CRAM-MD5 | `mailsrvr.cpp:4472` | removed |
| POP3 APOP | `mailsrvr.cpp:1397` | removed |
| IMAP CRAM-MD5 | `imapservice.js:961-1007` | removed; needs the transport work first |
| HTTP Digest | `websrvr.cpp:1918` | removed (see Open questions) |
| Hotline | `hotline.js:982-1020` | breaks; password is used as key material |
| MQTT broker PSK | `broker.js:2665`, `mqtt_broker.cpp:86-116` | breaks; enumerates sysop passwords |
| IRC door `PASS` | `irc.js:94,140` | breaks; hands password to the IRC server |

A transitional encrypted store "for sysops who still want CRAM-MD5" was
considered and **rejected**: under this threat model any user whose password
remains recoverable receives none of the protection. The deprecation is not
cleanup accompanying the real work; it *is* the mitigation. Since the storage
change is mandatory, these removals are unconditional — there is no mode in
which the mechanisms still function.

The bottom three rows are a different case and are treated separately under
"Passwords used where a key or token belongs": those sites are not using the
password to authenticate a user at all.

### Per-mechanism verifiers were considered and rejected

The stronger version of "keep CRAM-MD5 anyway" is not storing cleartext but
storing a **per-mechanism verifier** alongside the password hash, updating each
one when the password changes.

The technique is real and this document does not dispute it. HMAC-MD5 pads the
key to 64 bytes and runs one compression each over `K^ipad` and `K^opad`, so
saving those two 16-byte MD5 states lets the server answer any future challenge
without holding the password. RFC 2104 §4 describes exactly this
precomputation, and Dovecot ships it (`hmac_md5_get_cram_context()`; the stored
CRAM-MD5 scheme is those 32 bytes). It is also genuinely better than cleartext:
a leak does not directly hand over the password.

The disagreement is not about whether the intermediates exist. It is about
whether they satisfy P1 and P2. They satisfy neither.

1. **They fail P1 outright.** The midstate is a deterministic, unsalted
   function of the password alone. Two users with the same password have
   byte-identical records, so the file answers "which accounts share a
   password" by inspection, with no computation at all. That is the specific
   property the exercise is meant to eliminate.
2. **They fail P2 by four orders of magnitude.** Testing a candidate costs two
   MD5 compressions — GPU-speed, and with no salt one precomputed table attacks
   every account at once. Worse, storing *several* verifiers leaves you with the
   security of the weakest: an attacker holding the file does not attack the
   PBKDF2 record, they attack the CRAM record and recover the same password.
   The expensive hash becomes decorative.
3. **It does not generalize.** APOP is `MD5(timestamp || password)`: the
   password is a suffix and the timestamp changes per session, so nothing is
   precomputable and the literal password is required. Hotline derives session
   and ECC keys from the password (`hotline.js:1018-1020`), which is key
   material rather than a verifier. HTTP Digest's HA1 works, but only with a
   pinned realm, and is again unsalted MD5. The technique covers CRAM-MD5 and
   conditionally HTTP Digest; nothing else.

RFC 2104 §4 makes the same classification this document does. Having described
the precomputation, it closes: *"We stress that the stored intermediate values
need to be treated and protected the same as secret keys."* The intermediates
are a performance optimization for a party that already holds the key, not a
way to stop holding it — which is why they are safe in a session and unsafe in
a file that circulates.

Dovecot's own documentation reaches the same conclusion about its own feature:
mechanism-specific schemes "often offer very little protection", and using one
means storing "either cleartext, or ... a mechanism-specific scheme that's
incompatible with all other non-cleartext mechanisms". Their recommendation is
ARGON2ID or BLF-CRYPT instead.

A related suggestion — derive the CRAM intermediates from the KDF output rather
than the raw password, so the expensive work is not wasted — cannot work. The
*client* computes HMAC-MD5 from the password as typed and has no way to learn
the KDF output or to run the KDF itself. That asymmetry is the whole problem.

### SCRAM is the correct successor

The protocol that does what per-mechanism verifiers are reaching for, and does
it soundly, is **SCRAM** (RFC 5802; SCRAM-SHA-256 in RFC 7677). The server
sends the salt and iteration count, the *client* performs the key derivation,
and the server stores only a salted, iterated verifier. That is
challenge-response over expensive hashed storage, with no password-equivalent
material at rest.

It is a new mechanism rather than a retrofit: existing CRAM-MD5 clients cannot
participate, because they have no way to run the KDF. Adding SCRAM-SHA-256 is
therefore the forward path for SMTP, IMAP and POP3 — it restores
non-cleartext authentication for clients that support it, without reintroducing
a cheap verifier. `src/hash/sha256.c` already provides the primitive; see
Constraints for its Windows build gap.

Adding SCRAM is **not** a prerequisite for this design. Plain authentication
over TLS is sufficient and is what the deprecation assumes. SCRAM is recorded
here as the right answer to "what replaces CRAM-MD5", to be scoped separately.

Out of scope, and not to be confused with the above: `binkit.js` /
`load/binkp.js:369-398` also performs CRAM-MD5, but keyed by FidoNet *node*
passwords from the SBBSecho configuration. That is a separate secret store and
is unaffected.

Unaffected mechanisms, which work unchanged against a hash because they perform
a comparison rather than a derivation: HTTP Basic (`websrvr.cpp:2142`), NNTP
`AUTHINFO` (`nntpservice.js:248`), the webv4 form login
(`webv4/lib/auth.js:183`), FTP (`ftpsrvr.cpp:2684`), and terminal login
(`login.cpp:102`).

## Storage

### A separate file: `data/user/pass.tab`

Credentials move out of `user.tab` entirely, into a new fixed-record file
indexed by user number, using the same seek-and-byte-range-lock idiom
(`userdatoffset()` at `userdat.c:332`).

Rationale, in order of importance:

1. **`user.tab` is the artifact that circulates.** It is mode 0644, exported
   over SMB to the other hosts, opened by the web server on the scrape-heavy
   paths, read by sysop scripts, and present in every backup. Removing
   credentials from it makes a copy of it worthless for the stated attack.
   `pass.tab` is touched only by the authentication path and can be 0600.
2. **No record-length change**, so `user.tab`'s 1000-byte stride is untouched
   and there is no multi-host flag day (see Constraints).
3. Room to grow into Argon2id later without revisiting the decision.

### The record length lives in the file, not in a header file

`user.tab`'s record length is a compile-time constant (`USER_RECORD_LINE_LEN`,
`userdat.h:36`), and Constraints below describes what that costs: the stride
cannot be changed without every host that shares the file changing at the same
instant. `pass.tab` must not repeat that mistake, and it is the file most
likely to need a longer record later — a move from PBKDF2 to Argon2id, or a
larger salt, changes the record size.

So the length is **self-describing**:

- **Record 0 is a header**, occupying one full record slot. User *N*'s record
  is therefore at offset `N * reclen`, and the offset arithmetic remains a
  single multiply — the header costs one slot, not a special case.
- The header carries a magic, a format version, and the record length, in the
  file's own tab-separated convention.
- Readers take `reclen` from the header on open. No code path may use a
  compile-time record length.
- A missing, truncated or unparseable header **fails closed**: the file is
  unusable and authentication is denied, rather than being read at a guessed
  stride against records that do not start where the reader thinks.

Initial record length: 256 bytes, LF-terminated, tab-padded, matching the
existing convention. Growing it later rewrites `pass.tab` at the new stride and
updates the header. Because every release from the first one takes the length
from the file, a host running older code still lands on record boundaries — and
if the *format* version has moved past what it understands, it fails closed on
a version check rather than silently misreading.

### Record format

Self-describing, PHC-style, so the cost factor can be raised later and records
upgraded opportunistically:

```
$pbkdf2-sha512$i=<iterations>$<salt-b64>$<digest-b64>
```

A record must carry its own algorithm, cost and salt. Mystic BBS's
documentation notes that changing its iteration count does not re-hash existing
passwords; that is the direct consequence of a non-self-describing field, and
it is permanent in a way the algorithm choice is not.

Two algorithm identifiers are defined:

- `pbkdf2-sha512` — input hashed as typed; passwords are case-sensitive.
- `pbkdf2-sha512-uc` — input uppercased before hashing; passwords are
  case-insensitive. Used only by migrated records (see Migration).

`pbkdf2-sha512` is written here as the concrete example, not as a settled
choice — see "The crypto library has to be chosen before anything is built on
it" under Constraints. The point of the format is that the identifier is *in
the record*, so an `argon2id` identifier can be added later without touching
anything that already exists.

Iteration count is configurable, defaulting far above Mystic's 1000 (their wiki
recommends "3000 or lower for performance reasons"; current OWASP guidance for
PBKDF2-HMAC-SHA512 is on the order of 210,000).

Two values need explicit representation, and conflating them reintroduces the
bug described under "The `user.tab` password field" below:

- **No password required** (guest, anonymous FTP) — an explicit marker.
- **Record missing or unparseable** — denies authentication. Fails closed.

### The `user.tab` password field

`USER_PASS` is **not** blanked. It is set to a fixed, unmatchable 40-character
sentinel.

This is a safety requirement, not cosmetics. `login.cpp:88` reads:

```c
if (useron.pass[0] || user_is_sysop(&useron)) {   /* prompt for password */
```

An empty password field means *no password is requested at all* for
non-sysops, and `ftpsrvr.cpp:2595` treats empty as the anonymous convention. The
three hosts sharing `/sbbs` do not upgrade atomically, so a stale or
rolled-back binary reading a blanked field would authenticate every user with
no password. A sentinel makes that host prompt and reject instead: the stale
failure becomes "nobody gets in" rather than "everybody gets in".

## API

`user_t` stops carrying a password. The `pass` member is removed from the
struct rather than left holding the sentinel, so that every consumer of it
becomes a compile error and none are missed silently. Authentication goes
through:

- `verify_password(scfg_t*, uint usernum, const char* str)` — compare only.
- `set_password(scfg_t*, uint usernum, const char* str)` — hashes and stores.

`pass.tab` is **not** read by `getuserdat()`. It is opened only on the
authentication path. Folding it into the general user lookup would double the
per-page `user.tab` open/close traffic that is already the bottleneck during
web-scrape storms.

In JavaScript, `user.security.password` becomes compare-only
(`js_user.cpp:226` get, `:730` set). Third-party breakage is accepted; this is
the largest compatibility surface in the project and there is no capability
negotiation to soften it.

## Case sensitivity

Passwords are effectively uppercase today. `newuser.cpp:165` states the
situation in the tree's own words:

```c
strupr(useron.pass);  /* passwords are case insensitive, but assumed (in some places) to be uppercase in the user database */
```

"In some places" is why `websrvr.cpp` computes two HA1 values (`:1918`
original-case, `:1930` lowercased) and why `imapservice.js:982-1006` and
`hotline.js:982-987` each try three case variants — they compensate for not
knowing the stored case.

Measured across all 1457 records of the live Vertrauen `user.tab` (aggregate
counts only): **1402 all-uppercase, 7 all-lowercase, 47 containing no letters,
1 empty, and zero mixed-case.**

Two consequences:

1. The case entropy is already gone from stored data. Hashing the uppercased
   form loses nothing that is not already lost.
2. Case sensitivity matters for this threat model and cannot be skipped
   long-term. A cracked `pbkdf2-sha512-uc` record yields `TR0UB4DOR`;
   converting that to the user's real password on another system costs 2^k case
   guesses, roughly 256 tries for an eight-letter password. That is negligible,
   so uppercase-normalized hashing surrenders most of the benefit.

Genuine case sensitivity can only come from fresh user entry.

`K_UPPER` must be removed from the password entry paths: `newuser.cpp:190`,
`logon.cpp:295`, `useredit.cpp:517`. It is the single point where
case-insensitivity is enforced on the change path, and it is not
sysop-configurable — a per-system case-insensitivity option would be an opt-out
from part of P2, which "Mandatory, not optional" rules out for the same reason
as the rest.

## Migration

**Strategy B-prime: bulk-convert immediately, upgrade only at deliberate
password entry.**

1. **Day one.** Every password is converted to `$pbkdf2-sha512-uc$...` in
   `pass.tab`, and `user.tab`'s `USER_PASS` field is replaced with the
   sentinel. Cleartext leaves the user database immediately. This step depends
   on no user action, which is what makes it address the threat.
2. **Legacy records verify case-insensitively**, indistinguishable from today's
   behavior. Nobody is locked out and nobody has to type uppercase.
3. **Upgrade to `$pbkdf2-sha512$` only where the user deliberately enters a
   password** and types it twice: an expiry-driven change, a voluntary change,
   or a one-time prompt.
4. Records never upgraded remain legacy indefinitely, which is acceptable.

The upgrade must **not** be triggered silently by a successful login. Because
input has been uppercased for decades, users have had no reason to type
consistently; re-hashing whatever they typed that day would lock in an
arbitrary case variant and lock them out on the next session, with no
explanation and nothing having changed on their side. That failure mode fails
loudly and gets the feature reverted, which protects nobody.

The existing forced-change machinery is the hook: `logon.cpp:261` already
drives expiry from `cfg.sys_pwdays` against the per-user `pwmod`, displaying
`text[TimeToChangePw]`, followed by the double-entry confirm loop at
`:295-303`. The upgrade prompt is that flow with a different trigger and
message. Any system with `sys_pwdays` set therefore upgrades itself over one
expiry cycle; it defaults to 0, so the one-time prompt covers everyone else.

`pwmod` must not be touched during bulk conversion — it drives expiry, and
resetting it would silently postpone every user's next scheduled change by a
full cycle.

### Scrubbing existing copies

Migration is incomplete until the existing cleartext copies are dealt with.
On the live Vertrauen system today there are **seven** additional full copies of
the cleartext password database:

```
data/user/user.0.tab  ...  data/user/user.4.tab   (daily rotation)
data/user/back/user.tab
data/back08/user.tab
```

Conversion must scrub the `USER_PASS` field in the rotated and backup copies,
or refuse to claim completion. Leaving them is equivalent to not migrating: the
threat model is a copy of the file, and these *are* copies of the file.

## Other cleartext sinks

Hashing `user.tab` is necessary but not sufficient. Every remaining path that
hands cleartext to an insider is in scope:

| Path | Site | Disposition |
|---|---|---|
| User editor displays password | `useredit.cpp:78-81` | remove |
| Prints every user's password | `badpasswords.js:15` | redesign; it audits against `password.can` |
| Setup checker | `chksetup.js:138` | remove |
| Baja `%pass` | `exec.cpp:62` | remove |
| MQTT PSK table (JS broker) | `broker.js:2665` | breaks with hashing |
| MQTT PSK table (built-in broker) | `mqtt_broker.cpp:86-116` | breaks with hashing |
| qtmonitor PSK key | `qtmonitor/settingsdialog.cpp:60-62`, `main.cpp:22` | off-box copy; see below |
| `SM_ECHO_PW` logging | `login.cpp:106`, `ftpsrvr.cpp:2686`, `websrvr.cpp:5888` | see below |
| Door dropfiles | `xtrn_sec.cpp:392,648,744,778,831,925` | hardest; see below |

This list is a snapshot taken during one pass over the tree, and should be
treated as such — the durable fix for the class is "Passwords used where a key
or token belongs", below, not chasing individual call sites. Two entries were
missed on the first pass and are worth naming for what they show:

**The built-in broker duplicates `broker.js`.** `mqtt_broker.cpp:86` walks every
sysop and stores `m_psk_table[alias] = tolower(user.pass)`, the same enumeration
`broker.js:2665` performs. Noting an oddity while here: `authenticate_psk()`
(`:110`) ignores the table's value entirely and compares against
`cfg->sys_pass`. The stored password is used only as the TLS-PSK key at
`:699-702` — so the table is a password *store*, not a password *check*, which
is exactly the misuse described below.

**qtmonitor puts a BBS password on the sysop's workstation.** It accepts the
sysop password as `--psk-key` (`main.cpp:22`) and persists it through
`QSettings` (`settingsdialog.cpp:60-62`), whose placeholder text is literally
"sysop password (lowercased)". That is a cleartext copy of a live credential in
a per-user config file on a machine outside the BBS entirely, and no amount of
work on `user.tab` touches it. It is fixed by giving qtmonitor a generated PSK
instead of a password.

**`SM_ECHO_PW`** logs both the attempted *and* the stored password into
`data/logs/` (`"FAILED Password attempt: '%s' expected '%s'"`). It is off by
default but sysop-flippable, and it writes near-misses of real passwords into
files that circulate far more casually than `user.tab`. Hashing removes the
"expected" half automatically; the attempted half should go with it.

**Door dropfiles** cannot be fully solved. DOOR.SYS field 14 and its
equivalents have a cleartext password slot, third-party doors read it, and the
format is not ours to change. Writing an empty field is the recommendation — a
door authenticating off the dropfile password is repeating what the BBS already
did — but it will break something.

## Passwords used where a key or token belongs

Several of the breakages listed above are not casualties of hashing. They are
pre-existing misuses that hashing merely exposes. A password is an
*authentication input* — something a human types, that the system checks once.
It is being used in these places as *key material* and as a *shared secret
handed to a third party*, both of which call for a generated credential that
was never in a user's head and is never typed.

| Site | Uses the password as | Should be |
|---|---|---|
| MQTT broker PSK (`mqtt_broker.cpp:86`, `broker.js:2665`) | a TLS-PSK key | a generated per-identity PSK in configuration |
| qtmonitor (`settingsdialog.cpp:60`) | the same TLS-PSK key, stored off-box | the same generated PSK |
| Hotline (`hotline.js:1018-1020`) | ECC and session key material | protocol-fixed; no in-band fix |
| Door dropfiles (`xtrn_sec.cpp`) | a secret passed to arbitrary third-party code | a per-session token, or nothing |
| IRC door `PASS` (`irc.js:94,140`) | a secret passed to an IRC server | a generated service credential |

The MQTT rows are the clear win: a generated PSK is strictly better than a
password even today, because it is high-entropy, revocable per identity,
scoped to one service, and not reused on any other system. That change is worth
making on its own schedule and does not depend on when hashing lands — it
removes a live cleartext credential from a sysop's workstation now.

The general principle, which should outlast this document: **certificates,
public keys, generated tokens and derived per-service keys are the right fit
for most of what currently reaches for `user.pass`.** Every site that stops
using the password stops being blocked by this design.

## Constraints

### `user.tab` record geometry

Records are fixed-length: `USER_RECORD_LINE_LEN 1000` (`userdat.h:36`), 999
content bytes plus LF. Random access depends on it (`userdat.c:332`), as does
`lastuser()` (`:1928`) and 1000-byte byte-range locking (`:349,367,379`).
Padding uses the field separator itself, a tab (`userdat.c:45`), so trailing
empty fields and padding are indistinguishable — appending trailing fields is
free. Overflow **fails** rather than truncating (`userdat.c:844`).

Measured on the live file (1457 users): content ranges 434 to 671 bytes, median
543. But summing each field's longest observed value gives 917 content bytes
plus 67 separators — **984 of 999**. The format has roughly 15 bytes of
headroom for a maximally-populated record.

This is why credentials go in a separate file rather than growing `LEN_PASS`.
A full PBKDF2-SHA512 PHC string is about 132 characters against today's 40,
which a maximally-populated record cannot absorb. Raising
`USER_RECORD_LINE_LEN` would work, and `upgrade_to_v320.c` is precedent for a
strided rewrite, but it changes the offset stride: VERT, BBS and GIT all read
the same file through smbd, so every host would have to upgrade simultaneously
or compute wrong offsets against every record.

### Cost of verification, and the DoS it creates

A deliberately slow KDF is not free to the server, and one protocol makes that
acute: **HTTP Basic re-authenticates on every request**. Running a
210,000-iteration PBKDF2 per request is both a latency problem and a denial-of-
service amplifier, since an attacker spends nothing to send requests that each
cost the server a full derivation. The web server already suffers under
scrape storms, so this is not hypothetical.

HTTP Basic cannot be replaced with a session token to avoid this. Basic is an
HTTP-layer authentication scheme: the browser holds the credential and resends
it on every request to the protection space, and the server has no way to tell
it to stop. Session tokens live at the application layer and are a property of
a different login flow — webv4's form login (`webv4/lib/auth.js`) is one, and
it genuinely does run the KDF once per session, but that is a separate code
path from Basic and does not rescue it.

The answer is **not** a weaker KDF, which would forfeit the point of the
exercise. It is:

- **A verification cache.** On a successful derivation, cache the result keyed
  by username plus an HMAC of the presented credential under a per-process
  random key, with a short TTL and a bounded size. Subsequent requests in the
  same window cost one HMAC instead of one PBKDF2. The cache is memory-only,
  dies with the process, and never contains the credential itself. This is what
  Dovecot's auth cache does, and for the same reason.
- **Rate-limit the paths that must run a derivation**, and count a failed
  verification against the existing login-attempt machinery before the KDF
  runs, not after.
- **Cap concurrent derivations** so a burst cannot saturate every worker
  thread.

Note what the cache does *not* do: it does not help the first request from each
new client, which is what an attacker actually sends. The rate limit and the
concurrency cap are the load-bearing parts; the cache is what keeps legitimate
Basic-authenticated browsing usable.

This applies to any protocol that authenticates per request or per connection
rather than per session; HTTP Basic is simply the worst case.

### The crypto library has to be chosen before anything is built on it

This is a prerequisite decision, not an implementation detail to settle later.
It determines the record format, the achievable algorithms, and — as below —
which programs can still be linked.

**The binding constraint is link topology, not API.** `userdat.o` is linked
into the standalone utilities as well as the servers, and two of them matter
here: `sbbsecho` and `makeuser` (`objects.mk`; rules at `GNUmakefile:249` and
`:269`, prerequisites at `targets.mk:215` and `:219`). They link `$(HASH_LIB)`,
`$(XPDEV_LIB)` and `$(ENCODE_LIB)` — **neither links cryptlib.** `makeuser`
sets a password from the command line (`makeuser.c:175`), so it must be able to
run the KDF. Putting the KDF behind cryptlib drags cryptlib into every utility
that touches the user database, on all three toolchains.

The candidates, against that constraint:

- **Extend `src/hash/`.** No new dependency, and it is already linked exactly
  where it is needed. `md5.c`, `sha1.c` and `sha256.c` are public-domain
  reference implementations; on Linux all three build into `HASH_LIB`
  (`targets.mk:210-227`). Two known costs: **`sha256.c` is not built on
  Windows** — there is no `src/hash/*.vcxproj`, hash sources are pulled into
  consumers individually, and `smblib.vcxproj` compiles only `md5.c` (`:222`)
  and `sha1.c` (`:223`) — and `mail_dkim.c:34` documents a `SHA256_CTX` type
  clash between this `sha256.h` and OpenSSL's, which any password code sharing
  a translation unit with libcrypto will hit. Gets PBKDF2 and nothing stronger.
- **cryptlib.** Mandatory for the servers already (`-DUSE_CRYPTLIB` in
  `sbbsdefs.mk`) and provides PBKDF2, but see the linkage problem above.
- **A vendored Argon2.** The only route to a memory-hard KDF, which is the
  meaningful upgrade over PBKDF2 for P2. New third-party code in `3rdp/`,
  buildable on MSVC, GCC and Clang, and linkable into the utilities.

Whichever is chosen, the self-describing record format means the choice is not
permanent for *existing* records — it is permanent only for how long the tree
carries the loser.

## What this delivers

Against the two properties the threat model is stated in:

- **P1.** Per-user salts mean a copy of the database no longer reveals which
  accounts share a password, which is information the current file gives away
  by inspection and which is useful to an attacker before any cracking starts.
- **P2.** Offline recovery of a plaintext password from a copied file becomes
  computationally expensive rather than free, and weak passwords — where reuse
  hurts most — get a meaningful cost applied.

And:

- Credentials are absent from the file that actually circulates.
- Every user of every installation gets this, because it is not optional.

What it does not deliver, which any user-facing claim must respect: it does not
stop a sysop impersonating a user on this BBS, it does not stop a sysop who
instruments the running system, and it does nothing about a password captured
in transit.

## Sequencing

Steps 1 and 2 are prerequisites in the strict sense: because the change is
mandatory and there is no rollback, they must ship *before* the storage change
reaches a sysop, not alongside it.

1. **Transport.** Add STARTTLS to `imapservice.js`, or an IMAPS listener, so
   port 143 has a usable authentication path once CRAM-MD5 is gone.
2. **Choose the crypto library** against the linkage constraint in Constraints,
   and land whatever build work it implies — including the MSVC `sha256.c` gap
   if `src/hash/` wins.
3. Settle the `pass.tab` header, record format, sentinel value, and no-password
   marker.
4. Add `verify_password()` / `set_password()`; convert all in-tree consumers to
   them. Make `user.security.password` compare-only.
5. Add the verification cache, the rate limiting, and the concurrency cap on
   paths that run a derivation (see "Cost of verification"). This must land
   **with or before** step 6; enabling an expensive KDF on a per-request
   authentication path without it introduces a denial-of-service vector.
6. Remove CRAM-MD5, APOP and HTTP Digest.
7. Convert the cleartext sinks, and move the MQTT/qtmonitor PSK onto a
   generated key (see "Passwords used where a key or token belongs"). The PSK
   work can be pulled ahead of everything else; it is independently worthwhile.
8. Implement migration, including scrubbing the rotated and backup copies.
9. Add the case-sensitivity upgrade prompt and remove `K_UPPER` from the
   password entry paths.

## Open questions

- **HTTP Digest disposition.** Remove it, or pin the realm to `sys_name` and
  store an HA1 verifier? The case for removing it is that HA1 is unsalted MD5
  and so fails P1 and P2 exactly as the CRAM-MD5 midstates do, and that its only
  stock use is two sysop-level `webctrl.ini` files
  (`webv4/pages/webctrl.ini`, `webv4/pages/More/webctrl.ini`), both gated
  `AccessRequirements = level 90` — a handful of sysops, on a mechanism that
  would be the weakest record in the file.

  For the record, since an earlier draft of this document got it wrong: **no
  browser has deprecated or removed HTTP authentication.** Basic and Digest are
  both still supported. What is true is narrower — RFC 7616 (2015) added
  SHA-256 variants and deprecated the MD5 one within the scheme, and browser
  support for the SHA-256 variants is essentially nonexistent, so in practice
  "Digest" means MD5. The argument has to rest on that, not on a removal that
  never happened.
- **Whether SCRAM-SHA-256 is scoped into this work or scheduled after it.** The
  design assumes plain authentication over TLS and does not depend on SCRAM.
  Sequencing step 1 is required either way.
- Whether `pwmod` should move to `pass.tab` as password metadata.

## Related

Not blockers, but found during this analysis:

- [#1206](https://gitlab.synchro.net/main/sbbs/-/issues/1206) — the default
  `Authentication` list advertises TLS-PSK, which is compiled out.
- [#1207](https://gitlab.synchro.net/main/sbbs/-/issues/1207) —
  `CRYPT_TLSOPTION_USED_PSK` collides with `CRYPT_TLSOPTION_SUITEB_256`.

## References

- [Dovecot password schemes](https://doc.dovecot.org/main/core/config/auth/schemes.html)
  — mechanism-specific schemes "often offer very little protection".
- [Dovecot CRAM-MD5 context storage](https://dovecot.org/list/dovecot/2008-April/030014.html)
  — the two 16-byte MD5 states, and how they are extracted.
- [RFC 2104](https://datatracker.ietf.org/doc/html/rfc2104) — HMAC. §4 is the
  implementation note describing precomputed intermediates, and stressing that
  they must be protected the same as secret keys.
- [RFC 7616](https://datatracker.ietf.org/doc/html/rfc7616) — HTTP Digest
  Access Authentication; adds the SHA-256 variants and deprecates MD5.
- [RFC 5802](https://datatracker.ietf.org/doc/html/rfc5802) /
  [RFC 7677](https://datatracker.ietf.org/doc/html/rfc7677) — SCRAM,
  SCRAM-SHA-256.
- [Mystic BBS password policy](https://wiki.mysticbbs.com/doku.php?id=config_user_password_policy)
  — PBKDF2-SHA512, three storage modes, iteration count not applied
  retroactively.
- OWASP Password Storage Cheat Sheet — current PBKDF2 iteration guidance.
