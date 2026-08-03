# QWK certificate authority

**Date:** 2026-08-02

**Status:** Design, not approved for implementation.

## Goal

Provide an automated private certificate authority (CA) for Synchronet QWK
networks.  A QWK node authenticates to its direct hub using its existing QWK
account and password, submits a certificate-signing request (CSR) in its REP
packet, and receives a signed delegated CA certificate in the hub's QWK
packet.

The result is a certificate hierarchy which follows the QWK tree.  A BBS may
issue certificates for its direct child BBSes, for its own TLS services, and
for its local users' TLS clients.  It may not use that authority outside its
DNS subtree.

This is a private PKI.  Its DNS names are real Internet DNS names, but its root
is not a public Web PKI trust anchor.  A general-purpose TLS client may use a
certificate only after it has been configured to trust the applicable QWK root.

## Non-goals

- Replacing QWK account/password authentication.  The QWK account remains the
  authority that authorizes issuance and recovery of its direct node's CA.
- Providing automatic recovery from compromise of the offline root key.
- Providing an online revocation protocol such as OCSP.
- Transporting a user's private key in QWK packets.
- Compatibility with legacy TLS implementations.
- Creating browser-public HTTPS certificates.

## Terminology

- **Root** is the offline, long-lived trust anchor for one QWK DNS domain.
- **Root-signed intermediate** is the online CA directly signed by the root.
  It has a thirteen-month lifetime and is an explicit exception to the
  seven-day lifetime policy.
- **BBS CA** is a seven-day delegated CA belonging to one QWK node.  It signs
  direct-child BBS CAs and local end-entity certificates.
- **Service certificate** is a non-CA TLS certificate for a BBS service,
  including IRC server-to-server links.
- **User certificate** is a non-CA TLS client-authentication certificate for
  one local BBS user.
- **Parent** and **child** describe one direct QWK link.  They do not describe
  a routed NetMail path.
- **Canonical node name** is the lower-case DNS name derived from a node's QWK
  ID and its parent node's canonical name.

## Trust and topology

The root defines the DNS suffix for the network, for example
`synchro.net`.  A node's canonical name is its lower-case QWK ID followed by
its parent's canonical name:

```text
syncnix.synchro.net
mybbs.syncnix.synchro.net
```

The parent derives this name from the authenticated QWK account and local QWK
link configuration.  A CSR never authorizes its own subject name, SAN, issuer,
or certificate type.

The certificate chain is:

```text
offline root
  -> 13-month root-signed intermediate
    -> 7-day BBS CA for a QWK node
      -> 7-day child BBS CA, service certificate, or user certificate
```

There is deliberately no policy maximum QWK-tree depth and no
`pathLenConstraint` on BBS CA certificates.  A BBS CA may issue another BBS
CA for any direct configured child, and that child may do likewise.  Validators
must enforce the DNS-subtree relationship at every BBS-CA edge: a child BBS
CA name is exactly one DNS label below its issuer BBS CA name.

A BBS CA may also issue a service or user certificate immediately below its
own canonical name.  The certificate profile and Extended Key Usage (EKU), not
the syntactic name alone, distinguish an end-user identity from a BBS node
whose QWK ID happens to be the same label.

### Root and intermediate compromise

The root key is offline.  Root-key compromise requires an out-of-band recovery
and reconfiguration of trust anchors; QWK does not attempt to automate it.

The root signs online intermediates valid for one year and one month.  A
compromise of one is serious because it permits continuous creation of fresh
seven-day certificates.  It is the intended operational use of the emergency
revocation mechanism described below.  The root-signed intermediate is thus a
high-value online key and must receive stronger storage and operational
protection than an ordinary BBS CA.

## Naming and certificate profiles

All DNS labels in this design are ASCII lower-case.  A certificate-enabled QWK
ID must be a valid DNS label after lower-casing: it begins with an alphabetic
character, is 2--8 characters long, uses only `a-z`, `0-9`, and `-`, and does
not end in `-`.  There is no lossy mapping from other DOS-valid QWK IDs.  A
link with an ineligible QWK ID cannot use this CA feature until its identity is
changed to an eligible ID.

Every issued certificate has:

- exactly one subject attribute, `CN`;
- exactly one `subjectAltName` entry, of type `dNSName`;
- a SAN value exactly equal to the canonicalized CN;
- no wildcard, IP-address, URI, email, or additional SAN entries; and
- `subjectKeyIdentifier` and `authorityKeyIdentifier` extensions.

The root is locally configured and may use the same subject/SAN convention,
but it is not received through normal QWK issuance.

| Profile | Subject CN and sole DNS SAN | Constraints and use |
| --- | --- | --- |
| Root-signed intermediate | configured root-domain CA name | `CA:TRUE`, critical `keyCertSign` and `cRLSign`; root-controlled online issuance |
| BBS CA | canonical node name | `CA:TRUE`, critical `keyCertSign` and `cRLSign`; issue direct child BBS CAs and local leaves |
| BBS service | canonical node name of the local BBS | `CA:FALSE`, `digitalSignature`, `serverAuth` and/or `clientAuth` as the service requires |
| User client | `<local-account-id>.<canonical-bbs-name>` | `CA:FALSE`, `digitalSignature`, `clientAuth` only; maps to one local account |

CA certificates have critical `basicConstraints` and critical `keyUsage`.
End-entity certificates have critical `basicConstraints = CA:FALSE` and the
profile's critical `keyUsage`.  CA certificates do not carry a TLS EKU.

Every issuer constructs subjects and SANs itself.  A parent issues a BBS CA
only for the direct QWK node configured under it.  A BBS constructs a user
certificate identity only from its own local account identifier and canonical
BBS name.  No caller may request aliases or arbitrary DNS names through this
mechanism.  Issuing another DNS name requires separate domain-control proof
and is outside this design.

## Cryptography and keys

TLS 1.3 is mandatory.  The initial mandatory profile is:

- Ed25519 certificate and CA signatures;
- X25519 ephemeral key exchange; and
- `TLS_AES_256_GCM_SHA384`.

X25519 ephemeral exchange provides forward secrecy.  It is independent of
certificate renewal and is not replaced by TLS `KeyUpdate`.

Each BBS uses separate key pairs for separate roles:

1. A BBS CA signing key, whose public key appears in its delegated BBS CA
   certificate.  It signs child BBS CAs and local leaves.
2. A service key for each BBS TLS service identity.  It is never a CA key.
3. A user client key, generated and retained by the user's client device.  The
   BBS receives only its CSR and signed public certificate.

The xptls-style wrapper is the sole application interface to OpenSSL and Botan
3 key operations.  It exposes provider-neutral key handles for generation,
signing, TLS use, persistence, and deletion.  It must preserve opaque or
provider-backed keys where available, avoid exporting private material through
ordinary application buffers, and use restricted local storage and memory
cleansing where an exportable software key is unavoidable.

On successful BBS CA renewal, the BBS generates a new CA key.  The previous
CA private key is retired immediately from signing and should be deleted or
made unusable.  Its certificate and public key remain available only until
all leaves it issued have expired, so existing user certificates can be
verified during their remaining valid interval.

## Validity, renewal, and clocks

All BBS CAs, BBS service certificates, and user certificates have a maximum
validity of seven days.  A child certificate's `notAfter` must be no later
than its issuer's `notAfter`, less the five-minute skew margin.  Root-signed
intermediates are valid for thirteen months.

A BBS starts a new BBS-CA CSR when three or fewer days remain on its current
BBS CA.  It retains and retransmits the same pending request until it receives
a matching reply.  Deployments whose normal QWK request/reply round trip can
exceed the remaining three days accept that certificate service may lapse; on
its next successful QWK exchange the node re-enrolls using its QWK account.

The QWK account/password is deliberately the complete enrollment and recovery
authority.  The parent must not require a signature from the current or a
previous BBS CA key on a request.  Therefore a BBS that has been offline for
more than a week can recover with a new key, while theft of its QWK account
credential also permits unauthorized re-enrollment.  That is an accepted
consequence of this trust model.

All validation uses UTC.  A certificate, CRL, or protocol timestamp may differ
from local time by at most five minutes.  The implementation must log and
reject materially implausible local or peer times rather than silently
backdating or extending validity.

## QWK enrollment protocol

This feature is a QWKnet direct-link extension, not a message protocol.
Certificate files are additional archive members, as QWKnet already supports
arbitrary files transferred between a node and a hub.  They must not be
represented as conference-0 NetMail, routed NetMail, REP control-message
subjects, or message attachments.

### Request and response files

A child puts `CERTREQ.DAT` in the REP packet sent to its direct parent.  A
parent puts `CERTREP.DAT` in the QWK packet returned to that child.

Both files use a versioned binary envelope.  The exact field encoding is
defined with the implementation, but it must be canonical, length-delimited,
and reject unknown critical fields.  Version 1 contains at least:

| File | Required fields |
| --- | --- |
| `CERTREQ.DAT` | protocol version; 128-bit cryptographically random request ID; requested profile; DER PKCS#10 CSR |
| `CERTREP.DAT` success | protocol version; request ID; issued DER certificate; required DER issuer-chain certificates, excluding the configured root |
| `CERTREP.DAT` failure | protocol version; request ID; stable machine-readable rejection code; diagnostic text suitable for logs |

The CSR's proof of possession proves control of its new public key.  It does
not identify the requester.  The authenticated QWK account and the parent's
direct-link configuration identify the requester and select the only permitted
BBS CA subject/SAN.

No private key is present in either file.  A BBS creates its own service
certificate locally after installing its BBS CA.  A user client generates its
own key and submits a CSR to its local BBS through the BBS's authenticated
user-enrollment interface; QWK is not a private-key distribution channel.

### Delivery, replay, and installation

QWK file delivery is store-and-forward and packets may be lost or repeated.
The parent persists the result for `(authenticated child QWK account, request
ID)` until the returned certificate expires.  A duplicate request ID must
return the original success or failure result, never issue a second
certificate.  The child persists pending request state and retries the exact
same CSR and request ID until it receives the matching response.

The recipient verifies the entire returned chain, subject/SAN, profile,
validity, and request-ID match before installation.  Installation is atomic:
write the new key/certificate/chain set durably, validate it from the
configured root, then make it available to new TLS handshakes.  A failed
installation leaves the previously working set active.

Initial and recovery issuance relies on QWK account authentication only when
the transport and hub ingest path protect the authenticated upload's
integrity.  If an attacker can replace an authenticated REP's `CERTREQ.DAT`,
the attacker can substitute its own public key.  QWK transport deployments
must therefore provide authenticated, integrity-protected upload; password
authentication without integrity protection is insufficient for certificate
enrollment.

## Certificate validation and authorization

All Synchronet consumers must validate:

1. The chain terminates at the configured QWK root, not merely any locally
   trusted CA.
2. Every certificate signature, CA constraint, key usage, critical extension,
   validity interval, and applicable CRL is valid.
3. Each BBS-CA descendant name is exactly one label beneath the issuer BBS CA
   canonical name.
4. The expected DNS identity equals both the sole SAN and sole CN.
5. The leaf EKU is appropriate for the connection role.

For an IRC server-to-server link, both peers present service certificates and
each side performs this validation against the same configured root.  A
service certificate used for mutual TLS has both `serverAuth` and `clientAuth`
EKUs.  A BBS CA certificate must never be used as an IRC service certificate,
and its CA private key must never be used in a TLS handshake.

For Telnet client-certificate authentication, root validation alone is
insufficient.  A BBS accepts a user certificate only if its direct issuing BBS
CA is in that BBS's locally retained current-or-unexpired-retired issuer set.
It must reject a client certificate issued by the root, a hub, sibling, child,
or any other CA sharing the root.  It then maps the validated user identity to
one local account.

## Session and link rollover

TLS validates a certificate during the handshake; changing the configured
certificate affects only later handshakes.  TLS 1.3 does not support
renegotiation, and `KeyUpdate` rotates traffic keys rather than certificates.

Every certificate-authenticated service must cap an established session at:

```text
earliest relevant presented certificate notAfter - five minutes
```

For mutual IRC links, this is the earlier of the local and peer service-leaf
expiry.  The network must reconnect before that deadline.

IRC implementations should support an optional controlled-handover capability:

1. Establish a parallel, mutually authenticated replacement TLS connection.
2. Confirm the same root and canonical peer identity.
3. Bind old and new links with a fresh handover nonce sent over the old secure
   link and confirmed on the new one.
4. Quiesce old-link state changes, synchronize the delta over the new link,
   and atomically transfer routing/state ownership.
5. Close the old link after mutual commit acknowledgement.

Message/event identifiers or sequence numbers are required during overlap to
avoid loss or duplicate processing.  Peers without the capability fall back to
a normal close, reconnect, and IRC resynchronization.  A replacement must use
a certificate with a later `notBefore` than the old presented certificate.

## Revocation

Routine leaf revocation is deliberately omitted.  Disabling a QWK account
stops renewal but does not invalidate an already issued certificate; the
normal residual exposure is at most seven days.

Emergency revocation is supported for every certificate type, although its
planned use is compromise of a root-signed intermediate.  It uses full,
standard, DER-encoded X.509 CRLs:

- Every CA has `cRLSign` key usage.
- CRLs are matched by issuer identity and Authority Key Identifier.
- Validators track CRL number and reject older replacements.
- Delta CRLs and indirect CRLs are not supported in version 1.
- `thisUpdate` and `nextUpdate` are enforced with the same five-minute skew
  margin.
- A root-signed CRL revoking a root-signed intermediate invalidates that
  intermediate and every chain beneath it.

`CRLS.DAT` is a versioned binary QWK-packet member containing one or more
independently signed DER CRLs.  Every certificate-aware QWK packet includes
the newest verified bundle applicable to its recipient.  Nodes retain the
newest verified CRL for each issuer.  CRLs are intentionally distributed in
QWK packets, not REP packets.

Each issued certificate carries its issuer's `cRLDistributionPoints` HTTPS
URL.  The configured root trust anchor also records the root CRL URL.  Version
1 does not perform automatic HTTP retrieval; the URL permits a future
independent distribution and caching feature without changing certificates or
QWK packet formats.

QWK-only delivery has an unavoidable limit: revocation takes effect at a node
when it receives and verifies the CRL.  A compromised intermediate controlling
the only route to descendants may withhold the new `CRLS.DAT`.  Affected nodes
need an alternate QWK route or an out-of-band update in that event.  The
future HTTPS CRL mechanism may reduce this limitation, but is explicitly
outside this implementation.

## Acceptance criteria

- A fresh QWK node receives a correctly named, seven-day BBS CA certificate
  after an authenticated direct QWK request; the returned certificate is
  usable to issue its own service and child certificates.
- A node offline beyond certificate expiry re-enrolls with a newly generated
  BBS CA key using only its authenticated QWK account.
- A parent rejects a CSR that attempts a different name, SAN, profile, or
  issuer from the account/configuration-derived BBS CA request.
- A duplicate `CERTREQ.DAT` returns the original response and creates no
  additional certificate.
- A user private key never appears in a QWK packet, server-side user record,
  ordinary application log, or certificate response.
- A Telnet BBS accepts its own valid user-client certificate and rejects an
  otherwise valid client certificate issued by a different BBS CA under the
  same root.
- An IRC peer rejects a chain rooted elsewhere, an incorrect peer name, a
  CA certificate presented as a service leaf, or a leaf lacking the required
  mutual-TLS EKUs.
- A certificate-authenticated connection cannot continue past the prescribed
  certificate-expiry deadline.
- A verified root CRL revoking an intermediate causes all chains through that
  intermediate to fail.  A node which has not received the CRL retains the
  documented QWK-only revocation limitation.
