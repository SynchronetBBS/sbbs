# Hashed password storage

**Date:** 2026-07-31
**Status:** Design, not yet approved for implementation

## Problem

Synchronet stores every user's password in cleartext, as field 47 of each
record in `data/user/user.tab` (parsed at `userdat.c:545`, written at
`userdat.c:822`).

## Threat model

The adversary is **someone in possession of a copy of `user.tab`, including a
sysop or co-sysop**. The asset being protected is not the user's access to
*this* BBS — it is the user's password *as reused on other systems*. BBS users
reuse passwords; a leaked user database burns accounts elsewhere.

Explicitly in scope:

- A copy of the user database taken by anyone with read access to `data/`.
- Backups, exports, and the rotated copies the BBS makes itself.
- A sysop or co-sysop acting on a copy of the file, offline.

Explicitly **out** of scope, and the design must not claim otherwise:

- A sysop impersonating a user on this BBS. They can simply set a new password.
- A sysop who instruments the running system to capture passwords at login.
  No storage design can prevent this.

## Why encryption was rejected

Encryption at rest protects against an attacker who obtains the data but not
the key. The named adversary has both: a sysop owns `ctrl/`, and every server
process must be able to recover the password to serve a login. Reversible
encryption therefore provides **zero** protection against this threat model,
regardless of where the key is placed or how the three hosts share it.

Only a slow, salted, one-way hash addresses the stated goal.

## Why the digest authentication schemes must go

Any challenge-response scheme in which the *server* verifies an HMAC or digest
keyed by the password requires the server to hold something password-equivalent.
No storage format changes this: the stored material may be a *substitute* for
the password rather than the password itself (see "Per-mechanism verifiers"
below), but it is always sufficient to authenticate, and always cheap to attack
offline. The affected mechanisms:

| Mechanism | Site | Fate |
|---|---|---|
| SMTP CRAM-MD5 | `mailsrvr.cpp:4472` | must become optional, default off |
| POP3 APOP | `mailsrvr.cpp:1397` | must become optional, default off |
| IMAP CRAM-MD5 | `imapservice.js:961-1007` | must become optional, default off |
| HTTP Digest | `websrvr.cpp:1918` | drop, or pin realm and store HA1 |
| Hotline | `hotline.js:982-1020` | breaks; password is used as key material |
| MQTT broker PSK | `broker.js:2665` | breaks; enumerates sysop passwords |
| IRC door `PASS` | `irc.js:94,140` | breaks; hands password to the IRC server |

A transitional encrypted store "for sysops who still want CRAM-MD5" was
considered and **rejected**: under this threat model any user whose password
remains recoverable receives none of the protection. The deprecation is not
cleanup accompanying the real work; it *is* the mitigation.

Consequently the switch is **per-system, not per-user**: enabling hashing
hard-disables the digest mechanisms for that system.

### Per-mechanism verifiers were considered and rejected

The stronger version of "keep CRAM-MD5 anyway" is not storing cleartext but
storing a **per-mechanism verifier** alongside the password hash, updating each
one when the password changes. For CRAM-MD5 this is concrete and deployed:
HMAC-MD5 pads the key to 64 bytes and runs one compression each over
`K^ipad` and `K^opad`, so saving those two 16-byte MD5 states lets the server
answer any future challenge without holding the password. Dovecot implements
exactly this (`hmac_md5_get_cram_context()`; the stored CRAM-MD5 scheme is
those 32 bytes).

This is genuinely better than cleartext: a leak does not directly hand over the
password. It is nonetheless rejected here, for three reasons.

1. **The verifier is a single unsalted MD5 compression.** Testing a candidate
   password costs two compressions, so recovery runs at GPU speed and is
   rainbow-table-able. With no salt, one table attacks all users at once and
   identical passwords are visibly identical across accounts.
2. **Storing several verifiers gives you the security of the weakest one.** An
   attacker with a copy of the file does not attack the PBKDF2 record; they
   attack the CRAM record and obtain the same password. The expensive hash
   becomes decorative. Since the stated goal is precisely to prevent offline
   recovery for reuse elsewhere, this forfeits the entire objective.
3. **It does not generalize.** APOP is `MD5(timestamp || password)`: the
   password is a suffix and the timestamp changes per session, so nothing is
   precomputable and the literal password is required. Hotline derives session
   and ECC keys from the password (`hotline.js:1018-1020`), which is key
   material rather than a verifier. HTTP Digest's HA1 works, but only with a
   pinned realm, and is again unsalted MD5. The technique covers CRAM-MD5 and
   conditionally HTTP Digest; nothing else.

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

Record length: 256 bytes, fixed, LF-terminated, tab-padded, matching the
existing convention.

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

Iteration count is configurable, defaulting far above Mystic's 1000 (their wiki
recommends "3000 or lower for performance reasons"; current OWASP guidance for
PBKDF2-HMAC-SHA512 is on the order of 210,000).

Two values need explicit representation, and conflating them reintroduces the
bug described under Constraints:

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

`K_UPPER` must be removed from the password entry paths when a system is
case-sensitive: `newuser.cpp:190`, `logon.cpp:295`, `useredit.cpp:517`. It is
the single point where case-insensitivity is enforced on the change path.

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
| MQTT PSK table | `broker.js:2665` | breaks with hashing |
| `SM_ECHO_PW` logging | `login.cpp:106`, `ftpsrvr.cpp:2686`, `websrvr.cpp:5888` | see below |
| Door dropfiles | `xtrn_sec.cpp:392,648,744,778,831,925` | hardest; see below |

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

The answer is **not** a weaker KDF, which would forfeit the point of the
exercise. It is:

- **Authenticate once, then carry a session token.** webv4 already has the
  machinery: per-user session state in `data/user/<####>.web`, via
  `getSession()` / `setSessionValue()` in `webv4/lib/auth.js`. The KDF runs on
  session establishment, not per request.
- **Rate-limit the paths that must run a derivation**, and count a failed
  verification against the existing login-attempt machinery before the KDF
  runs, not after.
- **Cap concurrent derivations** so a burst cannot saturate every worker
  thread.

This applies to any protocol that authenticates per request or per connection
rather than per session; HTTP Basic is simply the worst case.

### Hash primitives

`src/hash/` provides `md5.c`, `sha1.c` and `sha256.c`, all public-domain
reference implementations. On Linux all three build into `HASH_LIB`, which is
linked into essentially every sbbs3 target (`targets.mk:210-227`).

**`sha256.c` is not built on Windows.** There is no `src/hash/*.vcxproj`; hash
sources are pulled into consumers individually, and `smblib.vcxproj` compiles
only `md5.c` (`:222`) and `sha1.c` (`:223`). Adding SHA-256 is a one-line
vcxproj change but is not free today.

`mail_dkim.c:34` documents a `SHA256_CTX` type clash between this `sha256.h`
and OpenSSL's; any password code sharing a translation unit with libcrypto will
hit it.

cryptlib is a mandatory dependency on every platform (`-DUSE_CRYPTLIB` in
`sbbsdefs.mk`) and offers PBKDF2 as an alternative to building it on
`src/hash/`.

## What this delivers

- Offline recovery of a user's plaintext password from a copied file becomes
  computationally expensive rather than free. This is the stated goal.
- Credentials are absent from the file that actually circulates.
- Weak passwords, where reuse hurts most, get a meaningful cost applied.

And what it does not deliver, which any user-facing claim must respect: it does
not stop a sysop impersonating a user on this BBS, and it does not stop a sysop
who instruments the running system.

## Sequencing

1. Settle the `pass.tab` record format, the sentinel value, and the
   no-password marker.
2. Add the hash primitives, including the MSVC `sha256.c` fix.
3. Add `verify_password()` / `set_password()`; convert all in-tree consumers to
   them. Make `user.security.password` compare-only.
4. Add the per-system hashing switch, which hard-disables CRAM-MD5, APOP and
   HTTP Digest when enabled.
5. Convert the cleartext sinks listed above.
6. Implement migration, including scrubbing the rotated and backup copies.
7. Add the case-sensitivity upgrade prompt and remove `K_UPPER` from the entry
   paths for case-sensitive systems.
8. Move HTTP Basic onto session tokens and add rate limiting and a concurrency
   cap on the paths that run a derivation (see "Cost of verification"). This
   must land with or before step 4; enabling an expensive KDF on a per-request
   authentication path without it introduces a denial-of-service vector.

## Open questions

- **HTTP Digest disposition.** Drop it, or pin the realm to `sys_name` and
  store an HA1 verifier? Its only stock use is two sysop-level `webctrl.ini`
  files (`webv4/pages/webctrl.ini`, `webv4/pages/More/webctrl.ini`), both gated
  `AccessRequirements = level 90`. Browsers have deprecated it.
- **IMAP without CRAM-MD5.** `imapservice.js:848-850` advertises
  `AUTH=CRAM-MD5 LOGINDISABLED` on non-TLS connections, and the service has no
  STARTTLS ("no STARTTLS support... deal with it"). Disabling CRAM-MD5 leaves
  port 143 with no usable authentication, so the deprecation must ship with
  either STARTTLS support or a migration to IMAPS. Adding SCRAM-SHA-256 would
  not resolve this on its own, since the objection to advertising a plaintext
  mechanism on an unencrypted port is unrelated to the storage question.
- **Whether SCRAM-SHA-256 is scoped into this work or scheduled after it.** The
  design assumes plain authentication over TLS and does not depend on SCRAM.
- **Algorithm.** PBKDF2-HMAC-SHA512 on `src/hash/` is the low-friction choice.
  Argon2id is stronger but is a new dependency on three platforms.
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
- [RFC 5802](https://datatracker.ietf.org/doc/html/rfc5802) /
  [RFC 7677](https://datatracker.ietf.org/doc/html/rfc7677) — SCRAM,
  SCRAM-SHA-256.
- [Mystic BBS password policy](https://wiki.mysticbbs.com/doku.php?id=config_user_password_policy)
  — PBKDF2-SHA512, three storage modes, iteration count not applied
  retroactively.
- OWASP Password Storage Cheat Sheet — current PBKDF2 iteration guidance.
