# Password storage

**Date:** 2026-07-31
**Revised:** 2026-08-01, following review by Deuce
**Status:** Design, not approved for implementation. **Hashing versus
encryption is an open decision** — see "Hashing or encryption".

The filename says "hashed password storage" because that is where the first
draft started. It is kept for continuity, but it overstates the state of the
discussion: the question was opened as *encrypted* password storage, encryption
remains under advocacy, and this document's preference for hashing is a
recommendation with an argument attached, not a settled decision.

The 2026-08-01 revisions restated the threat model as two named assets and
three testable properties, recorded the decision that the change is mandatory
rather than optional, corrected several errors and withdrew several bad
arguments against encryption, completed the audit of password sinks, settled
the `pass.tab` record format, and promoted the crypto-library choice and the
IMAP transport work to prerequisites.

## Problem

Synchronet stores every user's password in cleartext, as field 47 of each
record in `data/user/user.tab` (parsed at `userdat.c:545`, written at
`userdat.c:822`).

Everything below concerns that file. It does **not** concern the password's
existence in memory during authentication, which no storage design changes and
which is described next, because confusing the two is how a storage fix comes
to be described as something it is not.

## The password exists in cleartext during authentication regardless

This bounds every option in this document.

Most of the protocols Synchronet speaks transmit the password itself and ask
the server to check it: terminal login over Telnet or SSH, FTP `PASS`, HTTP
Basic, and SMTP/POP3/IMAP `PLAIN` and `LOGIN`. For those, the server receives
the cleartext by definition of the protocol. In the terminal case it lands in a
stack buffer — `char str[128]` in `sbbs_t::login()` (`login.cpp:31`), filled by
`getstr()` at `:63` and `:95` — and equivalents exist on every other
authentication path.

So at some instant, on every login, the plaintext password is in the process's
memory. Consequences that follow, none of which hashing or encryption alters:

- **A core dump can contain a live password.** This is not hypothetical for
  this project: crashes are captured deliberately, and cores are large. Any
  core from a server that was handling a login may hold one.
- **It can reach swap**, and from there the disk, outliving the process.
- **Zeroizing the buffer after use is hygiene, not a solution.** The window
  can be shortened, not closed.
- **This is what makes a malicious sysop unmitigable** for password
  authentication, as recorded under the threat model. It is the same fact
  stated from the other end.

The only real escape is to stop transmitting the password: SCRAM sends a proof
instead, and FIDO2 and public-key authentication never involve one. That is the
subject of "What would actually defeat a malicious sysop", and it is out of
scope here.

None of this weakens the case for changing storage. The two are independent: a
password that exists for microseconds in RAM during one login is a different
exposure from one that sits in a world-readable file forever, in every backup,
for every account at once.

## Threat model

The adversary is **possession of the user database**, by whatever route: a copy
taken by anyone with read access to `data/`, a backup, one of the rotated
copies the BBS makes itself, a co-sysop, a resold drive, a breach. The
adversary is the file, not a person with a job title.

Two assets are at stake, and both are real:

1. **The user's password as reused on other systems.** This is the primary
   asset, because the harm is unbounded and unfixable from here: BBS users
   reuse passwords, the accounts burned are on systems this BBS has no
   relationship with, and neither the sysop nor the user has any way to learn
   it happened.
2. **Every account on this BBS.** A leaked `user.tab` today is directly usable
   to log in as any user on every server the BBS runs — with the partial
   exception of sysop accounts, which have a second factor; see below. This is
   secondary only because it is bounded and recoverable — the damage stops at
   one system and a mass password reset ends it — not because it is small.

The second asset is easy to argue away by imagining the holder of the file is a
sysop, who could set a password anyway. Most holders are not: someone with read
access to `data/`, whoever ends up with a backup, whoever buys the drive. For
them the file is a complete account takeover, and hashing takes that away
outright rather than merely making it expensive.

### The system password already mitigates part of asset 2

Sysop accounts have a second factor that ordinary accounts do not: the
**system password** (`cfg.sys_pass`), stored separately from the user database
as the root `password=` key in `ctrl/main.ini` (`scfgsave.c:171`). It gates
sysop login when the sysop enables it — `login.cpp:115`, conditional on
`SM_SYSPASSLOGIN` and `SM_R_SYSOP` — and it gates the issuing of sysop commands
unconditionally, through `chksyspass()` (`con_hi.cpp:145`), subject to a
`sys_pass_timeout` re-prompt window.

On a system that requires it, a copy of `user.tab` yields the sysop's *user
account* but not the sysop's *authority*. That is a genuine mitigation and it
exists today.

It is bounded, though, in three ways that keep it from displacing anything
here. It is a single shared secret rather than a per-account credential, so it
protects nothing once it is itself disclosed — and it is cleartext too, in a
file that travels in the same backups. It does nothing for asset 1, the reused
password. And it does nothing for the other 1,456 accounts.

**The system password's storage and use are out of scope for this design.**
Nothing below changes where it lives, how it is compared, or when it is
demanded. It appears here because it changes what a leaked `user.tab` costs a
sysop, not because it is being touched.

The goal reduces to three properties of the stored data. They are the
acceptance test for every proposal in this document, and nothing below is
justified by assertion where it can be justified by these:

- **P0 — non-usability.** The stored record must not itself be presentable as
  a credential. This is the one that protects asset 2, and today's format fails
  it completely: the field *is* the password. Note what it does and does not
  say — it is about the record alone, not about the record plus a secret the
  server holds, so a one-way function satisfies it but so does an encrypted
  record. It rules out cleartext and the CRAM-MD5 midstate, and nothing more.
- **P1 — indistinguishability.** Two users with the same password must not
  produce the same stored record. A file that answers "which of these accounts
  share a password" is a map of which accounts to attack together, and it leaks
  even when nothing is cracked. This requires a per-user salt.
- **P2 — non-correlatability.** Given the file and a candidate plaintext, it
  must not be cheap to confirm whether that plaintext is the user's password.
  This is what stops a copy of the database from being turned into working
  credentials elsewhere. This requires a deliberately slow KDF.

P0 is free once P1 and P2 are met, which is why the rest of this document
argues about the latter two. It is stated separately because it is the property
that decides asset 2, and because it is possible to satisfy it and still have
achieved nothing: an unsalted fast hash meets P0 and fails both P1 and P2.

Explicitly **out** of scope, and the design must not claim otherwise:

- A **sysop** impersonating a user on this BBS. They can simply set a new
  password. This does not generalize to other holders of the file, per asset 2
  above.
- A malicious sysop generally. Anyone who can instrument the running system
  captures passwords as they arrive, and no storage design prevents it. What
  this design addresses is the *careless* handling of a database that a
  malicious party later obtains — which is how these files actually escape.

  This limit belongs to **password** authentication, not to authentication.
  Client-held-secret schemes really do defeat a malicious sysop: an SSH host
  never sees the private key, and a TLS client certificate is the same
  arrangement. SCRAM is the weaker member of that family — the password still
  exists, but the server never receives it. Saying "a malicious sysop cannot be
  mitigated" is therefore true only inside the scope of this design, and must
  not be quoted as a general claim.
- The live authentication path. The server necessarily sees the password at
  login. P0, P1 and P2 are properties of data at rest.

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

## What this design is, and the fair criticism of it

**An analysis that meets every existing use of passwords with "passwords, but
stored more carefully" is a narrow answer to a broad problem, and pairing it
with the removal of working features makes the narrowness harder to excuse.**

Most of that is correct.

A large share of the sites catalogued below should not be using a password at
all, and no amount of care in storing one fixes that. The MQTT broker wants an
issued API key. qtmonitor wants the key it was issued, not a human's login.
Door dropfiles want a per-session token or nothing. Hotline wants key material
that was never a password. The human-facing surfaces want FIDO2 on the web,
public keys at the terminal, client certificates for mail. Those are the better
analysis, they are written up under "Passwords used where a key or token
belongs" and "What would actually defeat a malicious sysop", and every one of
them is a larger improvement than this design is.

What defends doing this anyway is that it is the **floor, not the ceiling**:

- **None of the better answers retires an existing password.** A BBS with a
  thousand user records has a thousand passwords on the day FIDO2 ships, and
  most of those users will never enrol a security key. Storage remains the only
  measure that reaches every account.
- **It needs no user action where nothing about the login changes.** For
  terminal, FTP, HTTP Basic and any client already sending plain credentials
  over TLS, the swap is invisible. This is most accounts on most systems, and
  no post-password mechanism can say the same — each of those waits on a
  thousand individual enrolments.

  It is **not** true across the board, and an earlier draft claimed it was.
  Anyone whose mail client is configured for CRAM-MD5 or APOP must reconfigure
  it, and some cannot. The deprecations below are user-visible work, so the
  advantage holds for the storage swap, not for the whole package.
- **It is a precondition, not a competitor.** SCRAM stores a salted iterated
  verifier — the same primitive this design builds. Doing this work is doing
  part of that work.

What the criticism should change, and does:

- **The removals are a cost, not a benefit.** CRAM-MD5, APOP, HTTP Digest,
  password-by-email and retroactive password auditing are not being "cleaned
  up"; they are being paid. A shorter feature list is not an achievement here.
  They are also the price of *hashing* specifically, not of moving credentials
  into a separate file — see "What reversibility is actually worth".
- **A server-side replacement is not the same as a replacement.** An earlier
  draft asserted that no mechanism is removed without a replacement path, with
  IMAP's transport work as the example. That rule is weaker than it sounds and
  is qualified below.

### Removing a mechanism can lower a user's security

The replacement for CRAM-MD5 and APOP is plain authentication over TLS. That is
a replacement only if the *client* can do TLS, and there is no audit of what
clients this software's users actually run. For a BBS the population skews
toward old software — which is frequently exactly the software that implements
APOP and CRAM-MD5 and has weak or absent TLS.

The concrete regression: a POP3 client with CRAM-MD5 and no TLS support. Today
it sends a digest over the wire and never transmits the password. After
removal it either stops working or sends the password in cleartext across the
network. **That is a transport-security loss inflicted on a subset of users in
exchange for an at-rest-security gain shared by all of them.** The trade may
still be right, but it is a trade, and calling the server-side work a
"replacement path" hides it.

Three things follow:

- **The claim is narrowed.** A server-side replacement is necessary, not
  sufficient. Whether a mechanism has a replacement is a question about
  deployed clients, and this document cannot answer it.
- **Refuse rather than downgrade.** If plaintext authentication is not offered
  on unencrypted connections, an affected client fails visibly instead of
  quietly sending a password in the clear. That converts a silent security
  regression into a support ticket, which is the better failure.
- **SCRAM does not rescue this population.** It gives a non-plaintext mechanism
  compatible with hashed storage, so a supporting client needs no TLS to avoid
  sending its password — but vintage mail clients do not implement SCRAM either.
  It helps modern clients, not the ones this problem is about.

Encryption avoids all of it, because CRAM-MD5 and APOP keep working and nobody
reconfigures anything. That belongs on the encryption side of the ledger.

### Which threat is worse

The trade above only resolves by comparing two threats directly: interception
during authentication, which CRAM-MD5 and APOP defend against, versus cleartext
passwords sitting on a sysop's consumer-grade storage. This is a judgment, not
a measurement, and it is the judgment this design rests on: **at-rest is both
the more likely and the more severe of the two.**

**Likelihood.** The at-rest exposure is not contingent on anything. It is
present on every Synchronet system right now, has been since each account was
created, and is in every backup. Interception requires an attacker positioned
on a network path, and the realistic adversary for a hobby BBS is not one. The
dominant credential-theft vector of the era is commodity infostealer malware on
consumer machines — precisely what a sysop's box is — and it works by scraping
files at rest. A file of `alias<TAB>password` pairs needs no positioning, no
timing, and no interest in this BBS specifically.

**Severity.** One interception yields one credential, for one user, for one
session. One copy of `user.tab` yields every user, every reused password,
retroactively to whenever the system was founded, with no detection and no
expiry.

**And the protection being defended is narrower than it appears.** CRAM-MD5 and
APOP cover SMTP, POP3 and IMAP. Terminal login, FTP and HTTP Basic transmit the
password in the clear today and those mechanisms do nothing for them
(`login.cpp:102`, `ftpsrvr.cpp:2684`, `websrvr.cpp:2142` are all direct
comparisons). For a BBS, terminal login is the main event. For a telnet user
the entire session is cleartext regardless, and the password is the least of
what is on the wire.

Two qualifications that cut the other way, and are not enough to reverse it:

- **CRAM-MD5 is weaker than "protects against interception" suggests.** An
  intercepted challenge and response is an unsalted MD5 HMAC over a known
  challenge, so it is offline-crackable. It defends against passive replay and
  casual snooping, not against an attacker with a wordlist.
- **The affected users currently have both exposures.** Their passwords are
  already in cleartext in `user.tab` *and* they have transit protection on
  mail. Hashing removes the first for everyone and worsens the second for some.

That is the trade in one sentence, and it is worth taking: it converts a
certain, permanent, every-account exposure into a contingent, per-session one
affecting a subset.

Two things follow, and the second is not negotiable:

- If the comparison came out the other way for a particular system — a sysop
  who is confident in their storage and whose users are on hostile networks —
  that is a real argument for encryption, which keeps both protections.
- **Refuse rather than downgrade.** A client that cannot do TLS must fail to
  authenticate, not be quietly handed a cleartext path. Silently moving
  CRAM-MD5 users to `PLAIN` on port 110 is the worst outcome available and is
  the default if nobody decides otherwise.

SCRAM is the answer that gives both, and the obstacle to it is purely client
support.

The ordering, if resources allowed only one: the post-password work
reduces how many accounts the storage question applies to. It never reduces it
to zero, and it does not begin until the first user enrols. This design is what
covers the interval, which for a BBS is likely to be the rest of its life.

## Hashing or encryption

**This is the open decision in this document, not a settled one.** The question
was raised as encrypted password storage; encryption is still argued for; the
recommendation below is hashing, with the argument attached so it can be
attacked. Everything downstream — `pass.tab`, the record format, the migration
— is compatible with either, since both store an opaque per-user credential
that only the authentication path interprets. What changes is what that
credential is and whether a key exists.

There is also a **hard constraint from the sysop side**, recorded here as a
requirement rather than a preference: any encryption scheme that requires an
operator to type a key at startup is infeasible. A BBS restarts when the
machine does, unattended, at three in the morning. A design that will not come
back up on its own is not a candidate, however good its cryptography. That
rules out the passphrase-at-startup variant immediately and leaves the key in
an OS facility or a file — see "Where the key would actually live".

### The two sides, side by side

Both branches cost something, and the costs are of different kinds — one is a
tail risk, the other is certain and immediate. The sections that follow argue
each in detail; this is the ledger they add up to.

| | **Hashing** | **Encryption** |
|---|---|---|
| P0 — record not presentable | satisfied | satisfied |
| P1 — equal passwords differ | satisfied (per-user salt) | satisfied (randomized ciphertext) |
| P2 — offline attack | expensive, but possible; weak passwords fall | **impossible without the key** |
| Failure on key disclosure | no key exists | **total and retroactive: every password, at once, including strong ones** |
| CRAM-MD5, APOP, HTTP Digest | **removed** | keep working, unchanged |
| Mail a user their password | **reset only** | keeps working |
| Audit accounts for weak/shared passwords | **set-time only; sharing unanswerable** | keeps working |
| Users must reconfigure a client | **yes, for CRAM-MD5/APOP users** | no |
| Transit security for a no-TLS mail client | **worse — cleartext or refuse** | unchanged |
| Key management | none | generate, protect, rotate, back up, never lose |
| Portable floor on a host with no TPM or keystore | nothing to protect | **key in a file beside the data** |

Neither column is empty, and the choice is not obvious from the table. It turns
on how the two kinds of cost are weighed: hashing's are certain, immediate and
felt by identifiable users; encryption's is a low-probability event with an
unbounded blast radius. This design weighs them under "Which threat is worse"
and comes out on the hashing side — but that is the judgment being made, and it
is the thing to argue with.

### Bad arguments against encryption, withdrawn

Earlier versions of this section made the same mistake four times, and all four
claims are **withdrawn**: that a sysop necessarily holds the key; that the key
would sit on the same disk, in the same backup, as the data; that two users
with the same password would encrypt to the same ciphertext; and that
encryption fails P0 by definition.

The last of those was hedged with "under any deterministic scheme", which makes
it true — deterministic encryption really does leak equality, which is why it
is a named category with known leakage — but it had no business being the
default assumption. Any competent implementation uses a fresh IV or nonce per
record, and then it does not hold at all.

The P0 claim failed differently, and worse. It was circular: it read the
property as "is a one-way function", then noted that encryption is not one.
That is not an argument, it is the conclusion restated as a premise.

The first three share a pattern: each evaluated encryption at its *worst*
instantiation and then rejected the primitive for the result. A key kept beside
the ciphertext with matching permissions is encryption done wrong, not
encryption; so is ECB. Rejecting a primitive on the assumption that whoever
implements it will misuse it is an argument available against anything,
hashing included — an unsalted single-round SHA-256 is "hashing" too, and it
would fail every property in this document.

The comparison has to be against the primitive as competently built, so what
follows is.

### Encryption, done properly

Assume the key is held outside the data file — in an OS secret facility or
sealed to a TPM, and *not* typed by a human, per the constraint above — and
the ciphertext is randomized per record. Give it full credit, because it earns
some:

- **It satisfies P0.** A ciphertext record is not a credential: it cannot be
  presented to any login prompt, and without the key it authenticates nothing.
  P0 remains a real property doing real work elsewhere in this document — it is
  what kills cleartext storage, and what kills the CRAM-MD5 midstate, which
  *is* directly presentable as a credential with no key at all. Encryption is
  simply not in that class.
- **It satisfies P1.** Randomized ciphertext means equal passwords do not
  produce equal records.
- Against a **file-only adversary it is stronger than hashing on P2**, not
  weaker. Without the key there is no offline attack at all, where a hash
  merely makes one expensive. A weak password survives a stolen ciphertext and
  does not survive a stolen hash.

So encryption, competently built, meets every property this document asks for,
and beats hashing on one of them.

### What reversibility is actually worth

An earlier draft asserted that reversibility is "a capability that will never
be used". That is false. Synchronet uses the ability to recover a password in
at least four places today, and every one of them survives under encryption and
dies under hashing:

| Capability | Site | Under hashing |
|---|---|---|
| CRAM-MD5, APOP, HTTP Digest | `mailsrvr.cpp:4472`, `:1397`, `websrvr.cpp:1918` | removed |
| Mail the user their forgotten password | `login.js:90-134` (`options.email_passwords`, default **true**) | reset only |
| Audit every account against `password.can` | `badpasswords.js:15` | set-time only |
| Audit every account for weak, obvious or **shared** passwords | `chksetup.js:126-165` | set-time only, and sharing becomes unanswerable |

`chksetup.js` builds `password_list[password]` across every account precisely
to find users who share a password. Making that question unanswerable is
**P1** — the stated goal, not a side effect. One of this document's own
properties exists to destroy a capability a sysop has today and may reasonably
value.

The argument for hashing is therefore not that reversibility is unused, but
that each of these capabilities is one Synchronet is better off without:

- **Digest authentication** is the subject of its own section below. Its
  verifiers are unsalted MD5 and it is why the password must be recoverable in
  the first place — keeping it is the thing being argued against, not an
  independent benefit. This is the weakest of the four to dismiss, though: for
  a client with CRAM-MD5 and no TLS, removing it means sending the password in
  the clear instead, which is a loss for that user. See "Removing a mechanism
  can lower a user's security".
- **Mailing a user their password** transmits a live credential in cleartext
  over a store-and-forward network to a mailbox the BBS does not control, and
  leaves it there. A reset link or a new random password does the same job
  without that. This is a practice worth ending on its own merits, and it is on
  by default today.
- **Retroactive password auditing** answers a real question, and answers it by
  keeping every password readable forever so that the sysop can occasionally
  inspect them. The same protection comes from enforcing quality *at set time*,
  which `check_pass()` already does on the interactive paths, and from
  rejecting `password.can` entries when a password is chosen rather than
  discovering violations later.
- **Detecting shared passwords across accounts** has no replacement. It goes
  deliberately, as P1.

That is a value judgment about which features should exist, not a fact about
what the code does, and a sysop who disagrees about any of the four has a real
argument for encryption.

The rest of the case for hashing does not depend on that judgment:

1. **Its failure mode is total and retroactive.** One key disclosure exposes
   every password ever stored, at once, including those of users who chose
   well. Hashing has no such mode — there is no single secret whose loss does
   that — and it degrades per account instead: the attacker pays for each one,
   and strong passwords never fall. When the asset is a file that may leak
   years from now, the difference between "graceful" and "total" is the whole
   question, and it is why being stronger on P2 does not settle it.
2. **Key management is work that hashing does not require at all**: a secret to
   generate, protect, rotate, back up without destroying the backup's value,
   and never lose. That is not a claim about implementer skill; it is a
   difference in how many things must go right. Its true size is in the next
   section, and it is smaller than this document once made it.

### Where the key would actually live

Encrypted storage puts **one** key into an OS secret facility — the master key
— not thousands of user credentials. The encrypted records stay in `pass.tab`
as ordinary file data. So the requirement is a single secret of perhaps 32
bytes, readable by an unattended service at boot, which is a use these
facilities are designed for; storing a credential per user is one they would be
hopeless at.

**Only two of these are actually part of an operating system.** An earlier
version of this section listed `systemd-creds` as what "Linux" offers and the
BSDs as having "nothing standard", which compares a package that happens to be
preinstalled against one that happens not to be, and calls the difference an OS
capability gap. It is not one.

- **DPAPI and the macOS Keychain are OS facilities**, present on every
  installation, usable with no additional software.
- **The Linux kernel keyring is a kernel facility**, and is disqualified on its
  own merits below.
- **`systemd-creds` is systemd**, which is not Linux — Alpine, Void, Devuan and
  OpenRC systems do not have it. It is preinstalled on the mainstream
  distributions, which is a packaging convenience, not a property of the OS.
- **TPM2 sealing is hardware plus `tpm2-tss`**, which is a port or package
  everywhere, the BSDs included.

So the real axis is *default availability*, not standardness, and the genuine
capability question is whether the machine has a TPM — which is a hardware and
firmware question that cuts across OS families. A Linux box without a TPM and a
FreeBSD box without a TPM are in the same position.

| Facility | Where it comes from | Unattended? | Fit for one service key |
|---|---|---|---|
| DPAPI (`CryptProtectData`, machine scope) | Windows, base | yes | Good. Not a store — it returns a blob you save yourself. Machine scope means *any* local process can unprotect it, so it defends a stolen file, not a local attacker. |
| Credential Manager (`CredWrite`) | Windows, base | per-account | Workable. A real store, per-user, with a per-item blob limit of a few kilobytes. Fine for one key, useless in bulk. |
| CNG / DPAPI-NG (`NCryptProtectSecret`) | Windows, base | yes | Good, and scriptable to SID-based descriptors. Newer, less familiar. |
| System Keychain | macOS, base | yes | Good. Daemons can read system-keychain items without a login session. |
| `systemd-creds` + `LoadCredentialEncrypted=` | systemd (preinstalled on mainstream Linux) | yes | **Best fit where present.** Host-bound, optionally TPM2-sealed, delivered into `$CREDENTIALS_DIRECTORY` at unit start with no operator. systemd 250+. |
| TPM2 sealing | hardware + `tpm2-tss`, a package or port on Linux **and** the BSDs | yes | Good, with a caveat: sealed data is bound to the machine, so restoring a backup onto new hardware loses the key and every password with it. |
| Kernel keyring (`add_key`/`keyctl`) | Linux kernel | **no** | **Wrong tool.** In-memory and does not survive a reboot, so something must re-provision it at every boot — which is the original problem. Quotas are small besides. |
| A mode-0600 file | anywhere | yes | The floor, and the honest default. Protects against a stolen backup or a wrongly-permissioned share; not against a local attacker who can read it. |

Measured on one Debian 13 host while writing this: `systemd 257`,
`systemd-creds` present, `/dev/tpm0` and `/dev/tpmrm0` available. The kernel
keyring's per-user quotas on that same host are `maxkeys` **200** and
`maxbytes` **20000** (`/proc/sys/kernel/keys/`), ample for one key — but its
lack of persistence across reboot is what disqualifies it, not its size. Every
other row is from documentation rather than measurement, and the Windows,
macOS and BSD entries should be confirmed by someone running those systems
before anything is built.

**The portable design does not name any of them.** Synchronet reads the key
from a location the sysop configures; how that location is protected is the
sysop's decision and their platform's business. A file is the floor and works
everywhere; a sysop who wants DPAPI, a Keychain item, `systemd-creds` or a
TPM-sealed blob supplies the key through that mechanism instead. Writing the
design against one platform's facility is what produced the false asymmetry in
the first place.

What remains a real cost:

- **The floor is a file**, and some installations will run on it, because that
  is what happens when the better option needs hardware or a package the sysop
  does not have. A design has to be honest that this is the common case, not
  the fallback nobody uses.
- **Multi-host installations provision, they do not federate.** The answer to
  several hosts sharing one user database is to install the same secret on each
  — an ordinary step, of the same kind as installing a TLS private key, which
  sysops already do. It is a manual step per host, repeated on rotation, with
  the secret outside any facility while in transit. Those facilities are also
  mutually unintelligible: a DPAPI blob means nothing to a Linux node, so
  "use the OS key store" is a per-host answer applied per host.

None of this is an argument against encryption. It is the cost side of reason 2
at its true size, which is: some, and less than this document once claimed.
Reason 1 — the failure mode — is what actually decides the recommendation.

Encryption is neither weak nor badly implementable. It solves a harder problem
than the one at hand, fails less gracefully when it does fail, and charges for
a capability this design would rather not possess.

### What would actually defeat a malicious sysop

Nothing in this section, and nothing in this design. That is a property of
password authentication: the user sends the secret and the server checks it, so
the server sees it.

The escape is to stop sending it. Public-key authentication — SSH's model, or
TLS client certificates — leaves the private key on the user's machine, and no
sysop, careless or malicious, can obtain what the server was never given.
SCRAM is the intermediate case: the password still exists in the user's head,
but the server receives a proof rather than the secret.

**FIDO2 is the version of this that users will actually adopt**, and it is the
strongest of the set. The private key lives in an authenticator — a security
key, or the platform's own TPM or secure enclave — and is not extractable even
by the user. The server stores a *public* key, so a leaked credential file is
not merely expensive to attack, it is inert: there is nothing in it to crack,
and no amount of offline compute converts it into a login. It is also
origin-bound, so it cannot be phished or replayed against another system, which
addresses the reused-credential asset by construction rather than by cost.

Its limit is transport, not merit. WebAuthn is a browser API, so FIDO2 covers
the **web** surface and nothing else on its own. The post-password map for a
BBS is therefore three protocols, not one: FIDO2 for the web, public-key
authentication for terminal access, client certificates for the TLS-wrapped
mail protocols. Each is optional and each is additive — none of them removes
the need to store the passwords of the users who do not use them, which is what
this document is about.

That is a larger change than storage, and it is not scoped here. It answers
"then what *would* work", and it places the hashing-versus-encryption question
above as a choice between two server-side options rather than a claim that
server-side is the only option.

## Mandatory, not optional

Hashed storage is **not** a sysop-selectable mode. There is no configuration
item that turns it off, and no supported path back to cleartext.

The argument is about who bears the cost and who makes the choice. The party
who benefits is the user — the one with a reused password, and the one whose
account on this BBS is in the leaked file — and that user cannot tell a BBS
that hashes from one that does not. There is no banner, no protocol signal,
nothing observable at login. Making it optional therefore assigns the decision
to the one party that bears none of the consequence.

Two supporting arguments:

- An optional mode has to remain reachable, so the cleartext path stays in the
  codebase, stays wired into every server, and stays something that has to be
  tested and kept working indefinitely. The deprecations below buy nothing if
  the code they remove has to be retained behind a flag.
- P0, P1 and P2 are properties of a **file**, and user databases circulate
  between systems and across time — backups, host migrations, sysop-to-sysop
  transfers. A per-system opt-out is a per-system hole that travels with the
  copy.

The consequences are real and belong in the same breath as the decision:

- **No hashing switch gets built.** An earlier draft of this design proposed
  one; nothing like it exists in the tree today, and none is added. The digest
  mechanisms are removed outright rather than gated, which is less work than
  gating them, but it means there is no escape hatch for a cautious sysop.
- **IMAP on port 143 breaks at upgrade** for CRAM-MD5 clients. That promotes
  STARTTLS or IMAPS from an open question to a hard prerequisite: it must ship
  before the deprecation, not alongside it.
- **There is no rollback.** A sysop who upgrades and finds a mail client broken
  cannot revert, because the cleartext is gone by then. That is precisely why
  the transport prerequisite is not negotiable, and why migration has to be
  boring.

## Why the digest authentication schemes must go — under hashing

**This section is conditional on hashing being chosen. Under encryption every
mechanism in it keeps working, unchanged.** An earlier draft stated the
argument unconditionally, which was wrong and made the deprecations look like
an inherent consequence of moving credentials out of `user.tab`. They are not.
They are the price of the hashing branch specifically, and they are the main
entry on that side of the ledger under "Hashing or encryption".

Any challenge-response scheme in which the *server* verifies an HMAC or digest
keyed by the password requires the server to hold something password-equivalent
at the moment of verification. A one-way hash cannot supply it. **Encryption
can** — it holds the password and can recover it, which is exactly the
reversibility the trade is about. So the constraint below is a constraint of
hashing, not of storage.

Within the hashing branch, the stored material may be a *substitute* for the
password rather than the password itself (see "Per-mechanism verifiers" below),
but it is always sufficient to authenticate, and always cheap to attack
offline. The affected mechanisms:

| Mechanism | Site | Fate under hashing |
|---|---|---|
| SMTP CRAM-MD5 | `mailsrvr.cpp:4472` | removed |
| POP3 APOP | `mailsrvr.cpp:1397` | removed |
| IMAP CRAM-MD5 | `imapservice.js:961-1007` | removed; needs the transport work first |
| HTTP Digest | `websrvr.cpp:1918` | removed (see Open questions) |
| Hotline | `hotline.js:982-1020` | breaks; password is used as key material |
| MQTT broker PSK | `broker.js:2665`, `mqtt_broker.cpp:86-116` | breaks; enumerates sysop passwords |
| IRC door `PASS` | `irc.js:94,140` | breaks; hands password to the IRC server |

A **hybrid** — hashed storage generally, with an encrypted copy kept "for
sysops who still want CRAM-MD5" — was considered and rejected. Any user whose
password remains recoverable receives none of the protection, so the hybrid
buys the failure mode of encryption and the deprecations of neither. Choosing
encryption outright is the coherent version of that preference.

Within the hashing branch these removals are unconditional: since hashed
storage is mandatory when chosen, there is no per-system mode in which the
mechanisms still function.

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
whether they satisfy P0, P1 and P2. They satisfy none of them.

1. **They fail P0 for their own mechanism.** The midstate is *by construction*
   sufficient to answer any CRAM-MD5 challenge. Whoever holds the file can
   authenticate to the SMTP, POP3 or IMAP endpoint as any user immediately,
   with no cracking at all. Storing a verifier removes the ability to reuse the
   credential elsewhere; it does not remove the ability to log in here, which
   is asset 2.
2. **They fail P1 outright.** The midstate is a deterministic, unsalted
   function of the password alone. Two users with the same password have
   byte-identical records, so the file answers "which accounts share a
   password" by inspection, with no computation at all. That is the specific
   property the exercise is meant to eliminate.
3. **They fail P2 by four orders of magnitude.** Testing a candidate costs two
   MD5 compressions — GPU-speed, and with no salt one precomputed table attacks
   every account at once. Worse, storing *several* verifiers leaves you with the
   security of the weakest: an attacker holding the file does not attack the
   PBKDF2 record, they attack the CRAM record and recover the same password.
   The expensive hash becomes decorative.
4. **It does not generalize.** APOP is `MD5(timestamp || password)`: the
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

The fixed-length record is deliberate and is kept: it is what makes a user
record addressable by arithmetic instead of a scan, and what lets a single
record be byte-range locked without locking the file. `pass.tab` needs both
properties for exactly the same reasons `user.tab` does, and adopts them
unchanged.

Rationale, in order of importance:

1. **`user.tab` is the artifact that circulates.** It is world-readable, opened
   by every server on nearly every request, read by sysop scripts and
   third-party utilities, copied by the BBS's own rotation, and present in
   every backup. Removing credentials from it makes a copy of it worthless for
   the stated attack. `pass.tab` is touched only by the authentication path and
   can be mode 0600.
2. **`user.tab`'s stride is not disturbed.** The record is nearly full: on a
   sample live file, the longest observed value of every field summed together
   came to 984 of the 999 available content bytes. A PBKDF2-SHA512 credential
   runs to roughly 145 bytes against today's 40, so growing `LEN_PASS` would mean
   widening `USER_RECORD_LINE_LEN` — a format change every reader of the file
   must make at once, or compute wrong offsets against every record. A separate
   file avoids the question entirely.
3. **It gives space back.** The vacated `USER_PASS` field holds a one-byte
   sentinel rather than up to `LEN_PASS` bytes of password, so the change
   returns up to 39 bytes of the record to other fields instead of consuming
   more. See "The `user.tab` password field".
4. Room to grow the credential record later without revisiting any of this.

### The record length is measured, not stored

No code path may use a compile-time record length for `pass.tab`. This is not a
criticism of `USER_RECORD_LINE_LEN` (`userdat.h:36`), which has been a fine
choice for a record whose fields are all human-scale, and which has been
changed before by a strided rewrite (`upgrade_to_v320.c`). `pass.tab` differs
in one respect that matters: its record size is set by a *cryptographic*
parameter, so it moves whenever the algorithm does — a switch to Argon2id, a
longer salt, a wider digest. It is the file most likely to need a new stride,
and least able to predict when.

The length is obtained by **measuring the first record**: read from offset 0 to
the first LF, and that byte count is the stride. There is no header record and
no stored length field.

A header was the alternative, and is worse here:

- **The offset arithmetic is unchanged.** Records stay at `(N - 1) * reclen`,
  exactly as `userdatoffset()` computes it. A header would have consumed a slot
  and forced every record to shift.
- **Nothing has to be written for it to work.** The geometry is a property the
  file already has, by virtue of being fixed-length and LF-terminated. A file
  written by code that never heard of this convention is still measurable.
- **There is nothing to keep in sync.** A stored length is a second copy of a
  fact, and second copies go stale — a rewrite that changes the stride and
  forgets the header field yields a file that lies about itself, which is worse
  than one that says nothing.
- **No file-level version is needed**, because versioning lives one level down:
  each record names its own algorithm and carries its own cost and salt. There
  is nothing a file header would have to say.

What it requires, and what must therefore be enforced:

- **No field may contain an LF.** The measurement assumes the first LF in the
  file is the record terminator. This holds today by construction — fields are
  text, padding is tabs — but it becomes an invariant the write path must
  guarantee rather than an accident.
- **Sanity-check the result before trusting it.** The measured length must fall
  within a plausible range, and the file size must be an exact multiple of it.
  A file that fails either check is refused rather than read at a guessed
  stride, and authentication fails closed.
- **An empty or absent file has nothing to measure**, so the compile-time
  default applies at creation only — the one place a constant is legitimate.

Initial record length: 256 bytes, LF-terminated, tab-padded, matching the
existing convention. Growing it later is a rewrite of `pass.tab` at the new
stride and nothing else; every reader, old or new, measures the new length on
its next open.

#### This is a convention, not a `pass.tab` feature

`user.tab` may want the same treatment whenever its own record has to grow, and
measuring costs it nothing to adopt:

- **It is a code-only change there.** Every `user.tab` in existence already
  answers 1000 to this measurement, so a reader that measures instead of
  assuming is compatible with every file that exists, immediately, with no
  conversion and no format bump. The stride can then be widened later by a
  rewrite alone — which is why this belongs in place *before* it is needed
  rather than during the emergency.
- **The measurement belongs in a shared helper** rather than in the password
  code — measure, range-check, verify the file size divides evenly, return the
  length or fail. Both files then get identical behavior from one
  implementation, and a third fixed-record file gets it for free.

`user.tab` is not touched by this design. This is recorded so the `pass.tab`
implementation is shaped as something reusable from the start.

### Record format

The record is **tab-separated**, like every other `.tab` file in the tree. The
credential inside it is **one opaque field**:

```
<normalization>\t<verifier>
```

Tab-separated, tab-padded, LF-terminated — the same conventions as `user.tab`,
which is what the extension claims. That much follows from the filename, and it
buys three things: the parse and write idiom already in `userdat.c` is reused
rather than a second string grammar being added to the tree; it composes with
the measured record length above, which depends on tab padding and LF
termination; and trailing fields can be appended later for free, because
padding is the separator itself — which is how `pwmod` could move here without
a format change.

#### Why the verifier is opaque and not split into fields

The obvious alternative is to break the credential into `algorithm`,
`parameters`, `salt` and `digest` as four real fields. It reads better and it
is wrong.

**Password libraries verify from the encoded string, not from its parts.**
`argon2id_verify(encoded, pwd, len)`, `crypto_pwhash_str_verify(str, ...)` and
every equivalent take the whole crypt-format string and parse it themselves.
Storing the parts means reassembling the string before every verification, and
reassembling it *byte-exactly* — including which base64 alphabet and padding
convention that library uses. That is a subtle, security-relevant bug waiting
to happen, in exchange for legibility.

**The four-field schema is not universal.** bcrypt already breaks it: its salt
and digest are a single fused radix-64 run in a nonstandard alphabet, not two
values. Splitting asserts a structure that the algorithms do not actually
share, so every future algorithm risks not fitting.

**You can never re-hash on import.** If a user base is ever migrated in from
other software, the plaintexts are gone by definition, so the only way to carry
those accounts is to store the foreign verifier verbatim and let the library
that understands it do the checking. An opaque field can hold a `$2b$`, a
`$argon2id$` or a `{SSHA512}` string unchanged. A schema of our own devising
cannot.

So the BBS does not parse the verifier at all. It reads a string, hands it and
the typed password to a verify function, and gets a yes or no.

What survives from the split-field version is the property that actually
mattered: **every record carries its own algorithm, cost and salt**, because
the crypt format carries them. Mystic BBS's documentation notes that changing
its iteration count does not re-hash existing passwords; that is what a fixed,
non-self-describing field costs, and it is permanent in a way the algorithm
choice is not. Raising the cost applies to new records without invalidating old
ones.

**One verifier per record, not several.** The format could obviously hold a
list, and there is exactly one use for that: per-mechanism verifiers, so
CRAM-MD5 keeps working. That is the thing this design refuses, for the reasons
under "Per-mechanism verifiers were considered and rejected". A schema that
makes it convenient is a schema arguing against the design it belongs to.

#### The other field

`<normalization>` is how the typed password is preprocessed before it reaches
the verifier — `none`, or `upper` for migrated records (see Migration).

This is deliberately *not* folded into the algorithm identifier as a
`pbkdf2-sha512-uc` pseudo-algorithm. Uppercasing is Synchronet's behavior, not
the KDF's; a crypt string produced elsewhere would never carry such a marker,
and inventing one would be the first step toward a private format again. It is
our field, so it is our column.

#### Sizing and special cases

A PBKDF2-SHA512 verifier in crypt form runs to roughly 140 characters — a
16-byte salt is 24 base64 characters, a SHA-512 digest is 88, plus the
identifier and cost. With the normalization field that is about 145 of the 255
available content bytes. Argon2id with a 32-byte digest is smaller. The initial
256-byte record is comfortable, and the length is measured rather than assumed,
so it is not a ceiling.

Two values need explicit representation, and conflating them reintroduces the
bug described under "The `user.tab` password field" below:

- **No password required** (guest, anonymous FTP) — a reserved token in the
  verifier field. It is a stated value, not an empty one.
- **Record missing, empty or unparseable** — denies authentication. Fails
  closed. In particular an all-padding record is *not* "no password required".

### The `user.tab` password field

`USER_PASS` is **not** blanked. It is set to a sentinel: a value chosen so that
no account plausibly holds it, and verified at migration to be one that none
actually does.

This is a safety requirement, not cosmetics. `login.cpp:88` reads:

```c
if (useron.pass[0] || user_is_sysop(&useron)) {   /* prompt for password */
```

An empty password field means *no password is requested at all* for
non-sysops, and `ftpsrvr.cpp:2595` treats empty as the anonymous convention. A
blanked field is therefore read by any code that predates this change — a
downgrade, a rolled-back binary, an unupgraded utility, or another installation
sharing the same user base — as "this account needs no password", and it
authenticates everyone. A sentinel makes that reader prompt and reject instead:
the stale failure becomes "nobody gets in" rather than "everybody gets in".

Note that this is also why the field cannot simply be dropped from the record:
padding is the field separator itself, so an absent trailing field parses as
empty, which is the failure above.

**Shorter than forty bytes.** Since after migration *every* record carries it,
the field's worst-case contribution drops from `LEN_PASS` (40,
`sbbsdefs.h:537`) to the sentinel's length, returning most of those bytes to
the rest of the record — which matters, given how little headroom the format
has left. How much shorter is settled below, and it is not as short as an
earlier draft assumed.

**Two earlier drafts justified the choice of value badly, and both arguments
are withdrawn.**

The first claimed the sentinel was unmatchable because `getstr()` accepts
nothing below `' '` (`getstr.cpp:568`), so a control byte could never be
entered. Not every password arrives through `getstr()`: HTTP Basic and
SMTP/IMAP `PLAIN`/`LOGIN` carry base64 that decodes to arbitrary bytes,
`makeuser` takes one from `argv` (`makeuser.c:175`), and the JavaScript path
takes whatever it is given. "Cannot be typed" was never "cannot be presented".

The second claimed `check_pass()` (`userdat.c:4302`) guarantees no account can
hold a value shorter than `MIN_PASS_LEN` (4, `sbbsdefs.h:538`). It does not.
`check_pass()` is policy applied on three paths — new-user signup
(`newuser.cpp:183`), the change-password flow (`str.cpp:890`), and an advisory
`system.check_pass()` offered to scripts (`js_system.cpp:2196`). It is not an
invariant of the file, and the sysop-facing tools are exactly where it is
absent:

| Write path | `getstr()` filter | `check_pass()` |
|---|---|---|
| Terminal user editor (`useredit.cpp:517`) | yes | **no** — any length, including one byte |
| Standalone UserEdit GUI (`useredit/MainFormUnit.cpp:157`) | **no** — a plain text field | **no** — any bytes, any length |
| `user.security.password =` (`js_user.cpp:729`) | **no** | **no** — `SAFECOPY` and write |
| `makeuser` (`makeuser.c:175`) | **no** — from `argv` | **no** |
| `makeuser.js` (`:204`) | **no** | **no** — only a `password.can` check |
| Third-party tools, imports, a text editor | — | — |

So neither "short" nor "unprintable" is enforced anywhere near the whole
surface. There are several user-editor programs, and the GUI one accepts
arbitrary characters at arbitrary length by construction.

**There is no enforceable invariant here, so the design must not rest on one.**

What makes a collision tolerable rather than impossible: the new code never
reads `USER_PASS` at all. A record whose stored value happens to equal the
sentinel affects exactly one account, and only on a reader that predates the
change. That is a bounded accident, not a systemic failure.

Migration therefore **verifies instead of assuming**: the conversion pass scans
every record for the chosen sentinel before writing any, and picks a different
one if it collides. That turns an unenforceable assumption into a checked fact
at the one moment it can be checked cheaply.

**Choose a value that is implausible on every path, not impossible on some.**
Two properties, for two different reasons:

- **Non-printable.** Not because control bytes cannot be entered — the GUI
  editor above accepts them — but because no human types one into a password
  field, no import from other software carries one as a password, and they
  render as a visible marker rather than as data when a sysop looks at the
  file. Draw from a safe control range such as `0x0E`–`0x1F`: clear of tab, LF
  and CR, which delimit the format, and clear of `0x01`, which Synchronet reads
  as an attribute code.
- **Randomized per installation, and long enough to resist guessing.** A fixed
  constant compiled into the tree is the dangerous case: on any reader that
  predates the change, *anyone* could present the published byte and
  authenticate as *any* user, with no access to the file at all. That is the
  universal-password failure the sentinel exists to prevent, reintroduced. A
  per-installation random value closes it, because an attacker without the file
  does not know what to present.

Those two pull against the record-space argument, and the space argument loses.
A single random byte has 256 possibilities; several random control bytes are
what actually resist an online guess. Eight of them is ample — the
login-attempt and rate-limit machinery applies on the old reader too, since it
is still Synchronet — and eight bytes against `LEN_PASS`'s forty still returns
most of the field to the rest of the record.

**And none of this achieves unmatchability** against the adversary this
document is about. A comparison against the stored field is a comparison
against a value that adversary has *read*, so they can present it back
regardless. Randomization defeats the attacker who read the *source*; nothing
defeats the one holding the file.

So the sentinel's job is narrower than "unmatchable". It converts the failure
from **"nobody is asked for a password, so everybody gets in"** into **"an
attacker needs both a copy of the file and a reader that predates the
change"**. The first is universal and requires no access at all; the second
requires the file — the very thing this design assumes has leaked — *plus* a
downgraded binary. A large reduction, not a guarantee.

## API

`user_t` stops carrying a password. The `pass` member is removed from the
struct rather than left holding the sentinel, so that every consumer of it
becomes a compile error and none are missed silently. Authentication goes
through:

- `verify_password(scfg_t*, uint usernum, const char* str)` — compare only.
- `set_password(scfg_t*, uint usernum, const char* str)` — hashes and stores.

`pass.tab` is **not** read by `getuserdat()`. It is opened only on the
authentication path. Folding it into the general user lookup would double the
`user.tab` open/close traffic, which the web server already generates per page
render and which a crawler can drive hard.

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

Measured across all 1457 records of one long-running system's `user.tab`
(aggregate counts only): **1402 all-uppercase, 7 all-lowercase, 47 containing
no letters, 1 empty, and zero mixed-case.**

Two consequences:

1. The case entropy is already gone from stored data. Hashing the uppercased
   form loses nothing that is not already lost.
2. Case sensitivity matters for this threat model and cannot be skipped
   long-term. A cracked `upper`-normalized record yields `TR0UB4DOR`;
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

1. **Day one.** Every password is converted to an `upper`-normalized record in
   `pass.tab`, and `user.tab`'s `USER_PASS` field is replaced with the
   sentinel. Cleartext leaves the user database immediately. This step depends
   on no user action, which is what makes it address the threat.
2. **Legacy records verify case-insensitively**, indistinguishable from today's
   behavior. Nobody is locked out and nobody has to type uppercase.
3. **Re-normalize to `none` only where the user deliberately enters a
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

Migration is incomplete until the existing cleartext copies are dealt with. A
BBS keeps several of its own, without the sysop doing anything: on one system
surveyed there were **seven** additional full copies of the cleartext password
database beside the live one.

```
data/user/user.0.tab  ...  data/user/user.4.tab   (daily rotation)
data/user/back/user.tab
data/back<N>/user.tab                             (version-upgrade backup)
```

Conversion must scrub the `USER_PASS` field in the rotated and backup copies,
or refuse to claim completion. Leaving them is equivalent to not migrating: the
threat model is a copy of the file, and these *are* copies of the file.

## Other cleartext sinks

Moving credentials into `pass.tab` is necessary but not sufficient. Every
remaining path that hands cleartext to an insider is in scope:

| Path | Site | Disposition |
|---|---|---|
| User editor displays password | `useredit.cpp:78-81` | remove |
| **Mails the user their password** | `login.js:90-134` | **remove; on by default today** |
| Prints every user's password | `badpasswords.js:15` | redesign; it audits against `password.can` |
| Setup checker audits every password | `chksetup.js:126-165` | redesign; enforce at set time |
| Baja `%pass` | `exec.cpp:62` | remove |
| MQTT PSK table (JS broker) | `broker.js:2665` | breaks with hashing |
| MQTT PSK table (built-in broker) | `mqtt_broker.cpp:86-116` | breaks with hashing |
| qtmonitor PSK key | `qtmonitor/settingsdialog.cpp:60-62`, `main.cpp:22` | off-box copy; see below |
| `SM_ECHO_PW` logging | `login.cpp:106`, `ftpsrvr.cpp:2686`, `websrvr.cpp:5888`, `con_hi.cpp:166` | see below |
| Door dropfiles | `xtrn_sec.cpp:392,648,744,778,831,925` | hardest; see below |

This list is a snapshot taken during one pass over the tree, and should be
treated as such — the durable fix for the class is "Passwords used where a key
or token belongs", below, not chasing individual call sites. Two entries were
missed on the first pass, and they show what the rest of the list looks like:

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
work on `user.tab` touches it. It is fixed by issuing qtmonitor an API key
instead of handing it a password — see "The MQTT case" below.

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
| MQTT broker PSK (`mqtt_broker.cpp:86`, `broker.js:2665`) | a TLS-PSK key | an issued API key |
| qtmonitor (`settingsdialog.cpp:60`) | the same TLS-PSK key, stored off-box | the API key it was issued |
| Hotline (`hotline.js:1018-1020`) | ECC and session key material | protocol-fixed; no in-band fix |
| Door dropfiles (`xtrn_sec.cpp`) | a secret passed to arbitrary third-party code | a per-session token, or nothing |
| IRC door `PASS` (`irc.js:94,140`) | a secret passed to an IRC server | a generated service credential |

### The MQTT case: API keys

The MQTT rows are the clear win, and the standard answer to them is **API key
generation** — the server issues a random credential to a client, records it,
and can withdraw it. That is a different object from a password in every way
that matters here:

- **It is issued to a client, not to a person.** qtmonitor on the sysop's
  laptop and qtmonitor on their desktop hold different keys, so losing one
  laptop revokes one key.
- **It is revocable and rotatable independently.** Today, revoking a sysop's
  MQTT access means changing the password they log into the BBS with, and it
  changes it for every other sysop's broker access at once, since
  `authenticate_psk()` compares against `cfg->sys_pass`.
- **It is high-entropy, so it needs no KDF** — there is nothing to guess — and
  it exists on exactly one system, so it cannot burn an account anywhere else.
- **It never has to be typed or remembered**, which is what makes generating a
  strong one free.

The residue: TLS-PSK is symmetric, so the broker must still hold usable key
material at rest. API keys fix the *reuse* and
*revocation* problems completely and the at-rest problem only partly. The clean
version is client certificates for MQTT, which is the same principle one step
further and needs no shared secret at all — consistent with the general rule
below.

None of this waits on hashing. It removes a live cleartext BBS password from
sysop workstations now, and can proceed on its own schedule.

The general principle, which should outlast this document: **certificates,
public keys, generated tokens and derived per-service keys are the right fit
for most of what currently reaches for `user.pass`.** Every site that stops
using the password stops being blocked by this design.

The same pattern shows up outside this design's scope, which is some evidence
it is a pattern and not a coincidence: `cfg.sys_pass` — a typed human password
— is what encrypts the server's private key in the cryptlib keyset
(`ssl.c:482`, `:549`). Out of scope here.

## Constraints

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

Against the properties the threat model is stated in:

- **P0 — and this is the part that lands immediately and completely.** A leaked
  file stops being a set of working credentials. Today, anyone who obtains
  `user.tab` can log in as any user on any of the BBS's servers, sysops
  included; afterwards they can log in as nobody. That is asset 2, protected in
  full, on day one of the migration, with no dependence on password strength.
- **P1.** Per-user salts mean a copy of the database no longer reveals which
  accounts share a password, which is information the current file gives away
  by inspection and which is useful to an attacker before any cracking starts.
- **P2.** Offline recovery of a plaintext password from a copied file becomes
  computationally expensive rather than free, and weak passwords — where reuse
  hurts most — get a meaningful cost applied. Unlike P0 this is a matter of
  degree: a bad password still falls, it just costs something.

And:

- Credentials are absent from the file that actually circulates.
- Every user of every installation gets this, because it is not optional.

What it does not deliver, which any user-facing claim must respect: it does not
stop a sysop impersonating a user on this BBS, it does not stop a sysop who
instruments the running system, and it does nothing about a password captured
in transit.

And what it costs, all of it a consequence of choosing **hashing** rather than
of moving credentials out of `user.tab`: CRAM-MD5, APOP and HTTP Digest stop
working; the BBS can no longer mail a user their forgotten password, only reset
it; and password auditing becomes a set-time check rather than something a
sysop can run across the existing user base. Encryption would keep all of them.
That is not a tidier feature list, it is the price. See "What reversibility is
actually worth".

## Sequencing

Steps 1 and 2 are prerequisites in the strict sense: because the change is
mandatory and there is no rollback, they must ship *before* the storage change
reaches a sysop, not alongside it.

1. **Transport.** Add STARTTLS to `imapservice.js`, or an IMAPS listener, so
   port 143 has a usable authentication path once CRAM-MD5 is gone. This is
   necessary and not sufficient — see "Removing a mechanism can lower a user's
   security" for what it does not cover.
2. **Choose the crypto library** against the linkage constraint in Constraints,
   and land whatever build work it implies — including the MSVC `sha256.c` gap
   if `src/hash/` wins.
3. Settle the `pass.tab` record format, the sentinel value, and the no-password
   marker; add the shared record-length measurement helper.
4. Add `verify_password()` / `set_password()`; convert all in-tree consumers to
   them. Make `user.security.password` compare-only.
5. Add the verification cache, the rate limiting, and the concurrency cap on
   paths that run a derivation (see "Cost of verification"). This must land
   **with or before** step 6; enabling an expensive KDF on a per-request
   authentication path without it introduces a denial-of-service vector.
6. Remove CRAM-MD5, APOP and HTTP Digest.
7. Convert the cleartext sinks, and move MQTT and qtmonitor onto issued API
   keys (see "The MQTT case"). That work can be pulled ahead of everything
   else; it is independently worthwhile.
8. Implement migration, including scrubbing the rotated and backup copies.
9. Add the case-sensitivity upgrade prompt and remove `K_UPPER` from the
   password entry paths.

## Open questions

- **Hashing or encryption.** The first and largest. This document recommends
  hashing and gives its reasons, but the question was raised as encrypted
  storage and encryption is still argued for. Everything else here is
  compatible with either. Settling it needs agreement on two things: whether a
  total, retroactive failure on key disclosure is acceptable in exchange for
  being immune to offline attack in the meantime; and whether it is acceptable
  that on a machine with no TPM and no OS keystore in use — which is the common
  case, on every platform — the key sits in a file beside the data it protects.
  The one thing already settled is that no scheme may require a human to type a
  key at startup.
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
  Sequencing step 1 is required either way, and SCRAM does not help the clients
  most affected by the deprecations, which do not implement it.
- **Whether plaintext authentication should be refused on unencrypted
  connections.** Refusing makes an affected client fail visibly; permitting it
  lets that client silently downgrade from CRAM-MD5 to a password sent in the
  clear. This design takes a position — refuse — under "Which threat is worse";
  what remains open is whether a sysop may override it.
- **What clients are actually in use.** The deprecations' cost cannot be
  estimated without this, and no audit exists.
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
