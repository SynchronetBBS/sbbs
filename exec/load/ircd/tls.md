# TLS server links

Server links can use a dedicated TLS port with certificate validation. TLS
starts immediately on that port; there is no STARTTLS command or plaintext
fallback. The initiating server checks the peer's hostname, certificate dates,
server-authentication usage and chain of trust before sending IRC passwords
or network state. TLS 1.2 is the minimum protocol version.

For example, on each hub, retain its existing plaintext listener for legacy
leaves and add a separate TLS listener:

```ini
[Port:6667]
Default=true

[Port:6697]
TLS=true

[TLS]
CAFile=ircd-ca.pem
```

`CAFile` is a PEM bundle of trusted CA certificates, not server certificates.
Relative filenames are resolved against the Synchronet control directory;
absolute paths can point to an administrator-maintained CA bundle. Configure
this on every server that initiates TLS links. Missing, unreadable or invalid
bundles prevent those links from connecting. Rehashing clears the cached CA
bundle; changing the bundle also causes it to reload on subsequent connections.
Existing connections are not revalidated when the file changes.

TLS listeners use Synchronet's existing `ctrl/ssl.cert` certificate and private
key, as other Synchronet TLS services do. Install a CA-issued certificate with
the appropriate hostname and intermediate certificates using Synchronet's
existing certificate-management tools. A newly generated self-signed server
certificate will not pass validation against a CA bundle.

Inbound handshakes use the native `Socket.acceptTLS()` API when available.
Private-key loading and TLS negotiation run in native workers; JavaScript
callbacks run on the IRCd's existing callback thread. The daemon allows up to
16 pending handshakes with a 10-second network timeout and closes excess
connections. Older hosts use synchronous TLS handshakes and log a warning once
per daemon run when the fallback is first needed. Those hosts remain vulnerable
to handshake stalls until the native executables and libraries are upgraded.
Both paths require successful TLS before IRC registration; a failed asynchronous
handshake is closed without retrying through the synchronous fallback.

Configure each hub's peer entry to require TLS:

```ini
[Server:other-hub]
Servername=other-hub.example.net
Hostname=other-hub.example.net
Port=6697
TLS=true
Password=replace-with-the-agreed-link-password
Class=30
Hub=true
```

Use an existing server-link class with a positive `ConnectFrequency` for
automatic connections. `Port=0` disables automatic connections while still
allowing inbound links; `/CONNECT` defaults to 6697 for such TLS entries.

`Hostname` is the name checked against the certificate and sent as TLS SNI.
It may differ from the IRC `Servername`; use a DNS hostname covered by the
certificate rather than an IP address unless the certificate covers that IP.
The peer's IRC handshake must identify the configured `Servername`.

`TLS=true` also rejects plaintext connections claiming that server name,
including connections arriving on 6667. Configure it on both ends to require
encrypted links whichever hub initiates the connection. It applies before
password or QWK authentication, including when wildcard server entries exist.
IRC link passwords still authenticate the connecting server; client
certificates are not required.

Other dedicated TLS ports work by setting `TLS=true` in both the local
`[Port:number]` section and the connecting hub's `[Server]` section. Ports 6697
and 994 always use TLS. Changing a `/CONNECT` port cannot turn off TLS for an
entry that requires it.

Entries on ordinary ports that omit `TLS` retain their existing plaintext
behavior. Existing plaintext clients, leaves and services do not need to
change. Older IRCds must support validated outbound TLS themselves to initiate
these protected links; upgrading the other end does not change their outbound
connection behavior.

`ircdcfg.js` exposes the CA bundle under **TLS Settings**, marks TLS listeners
in **TCP/IP Ports**, and provides **Require TLS** in each server entry. Listener
additions require an IRCd restart; server entries and the CA bundle can be
rehashed. A failed link logs its validation failure and uses the existing
reconnect interval.

The checks validate the presented certificate chain against local trust;
they do not perform online OCSP or CRL lookups.

## Route security

Any registered client can inspect the route from its IRCd to an exact server
name (case-insensitive):

```irc
/QUOTE TLSROUTE other-hub.example.net
```

The server target also accepts wildcard masks, such as `other-hub.*`; the first
matching server is selected using the usual server lookup. Replies name that
server explicitly.

Server NOTICEs list each hop and end with `TLSROUTE END`. The summary is
`all network hops TLS`, `plaintext present`, or `unknown`. A local target has
`no network hops`. The client's own connection is excluded. Embedded services
hosted inside an IRCd have an explicit `INTERNAL` hop, which is excluded from
network-hop TLS totals.

Certificate validation is reported separately. `VERIFIED by server.example`
means that connecting endpoint successfully checked the accepting server's
certificate. It does not mean mutual certificate authentication. Incoming TLS
alone cannot establish whether the connecting endpoint checked the certificate;
absent a trusted report, that check is `UNKNOWN`. Contradictory reports cannot
produce a verified result; any plaintext evidence makes the summary
`plaintext present`.

This feature uses actual socket observations and reports relayed by configured
hubs, not inference from ports, versions, or configuration. Remote reports are
assertions by trusted hubs, not independently verifiable cryptographic proof.
Untrusted leaves (including U:Lines without hub authority) cannot vouch for
security elsewhere. Consequently, their outbound certificate checks remain
unknown to the network even when the hub can confirm the connection uses TLS.
Internal-service reports from untrusted hosting leaves also remain unknown
remotely.

The optional server capability `LINKSEC` carries observations in the form
`:<reporter> LINKSEC <neighbor> <TLS|PLAIN|INTERNAL> <OUT|IN|NONE>
<VERIFIED|UNVERIFIED|NA>`. The reporter must be one endpoint of a live adjacency.
Certificate status applies to TLS `OUT` reports; other reports use `NA`.
`INTERNAL NONE NA` is reported only by the hosting endpoint. Observations are
sent after admission and replayed after topology introductions during bursts;
there is no periodic probing. State lives on the topology edge and disappears
with that edge on disconnect.

Older IRCds receive no new commands and keep working. An updated server can
report its direct link to an older peer, but remote hops hidden behind legacy
intermediaries remain unknown. No TLS policy or required capability changes
when route reporting is enabled.

## WHOIS security

Version 2.8 reports the complete path between the requesting client and the
target client in WHOIS. Numeric **671** means both client connections and every
intervening server connection are known to use TLS. Numeric 320 instead
explains a known unencrypted connection or an unknown path. The server tree
path excludes unrelated uplinks above the two clients' common ancestor.
Server-qualified WHOIS queries are assessed at the requesting client's server,
which suppresses older servers' endpoint-only 671 replies.

The optional `CLIENTTLS` capability carries
`:<owning-server> CLIENTTLS <nickname> <TLS|PLAIN|INTERNAL>` immediately after
each NICK introduction, including bursts. Only the user's owning server may
report that user's connection, over its established route. Leaves may report
their own clients; this does not grant them authority over server links or
other servers' clients. INTERNAL is restricted to services and daemon helpers.
Conflicting reports cannot establish TLS, and observations disappear with the
user object on quit or split. Nick changes retain the same connection state.

Older peers receive no CLIENTTLS commands. A legacy reporting gap remains
unknown, even if a previous WHOIS 671 described the target's socket as secure.
This reporting is also used by the optional embedded-services TLS requirement;
see [services.md](services.md#requiring-tls).

## Tests

Run the configuration and handshake regressions with Synchronet `jsexec`:

```sh
jsexec -n -c /path/to/private/ctrl -i exec/load exec/tests/ircd/tls.js
jsexec -n -c /path/to/private/ctrl -i exec/load exec/tests/ircd/link-security.js
```

`tests/ircd/tls-network.py` exercises real native Socket/CryptCert validation,
automatic and manual links, separate TLS/plaintext listeners and legacy leaf
connections. It also checks three-hub TLS routes, burst replay, reconnects that
change transport, legacy reporting gaps and untrusted leaf claims. It requires
Python's `cryptography` package and the private
fixture prepared by `tests/ircd/services-fixture.py`:

```sh
python3 exec/tests/ircd/tls-network.py --jsexec /path/to/jsexec
```

The native tests check rejection of untrusted issuers, peer-supplied untrusted
roots, self-signed certificates, wrong names, invalid dates, inappropriate
certificate usage and invalid CA bundles, and verify that no IRC password is
sent on rejected links. All test certificates and logs remain under the
printed temporary directory.
