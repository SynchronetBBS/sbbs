# Channel exceptions (IRCd 2.9)

Channel operators manage the `+e` list with ordinary MODE commands:

```text
/MODE #channel +e Nick!user@host
/MODE #channel +e ~msgbypass:external:Bot!*@*
/MODE #channel +e ~msgbypass:moderated:Bot!*@*
/MODE #channel -e ~msgbypass:external:Bot!*@*
/MODE #channel +e
```

A plain hostmask exempts matching users from channel bans (`+b`) when joining
or speaking. It does not bypass invite-only, keys, limits, moderation, or the
restriction on outside messages.

`~msgbypass:external:<hostmask>` bypasses `+n` for matching senders.
`~msgbypass:moderated:<hostmask>` bypasses `+m`. Both apply to PRIVMSG and NOTICE.
An outside sender to a `+mn` channel needs both entries. A ban still applies
unless a separate plain exception matches. These entries confer no membership,
channel visibility, or subscription to channel messages.

The short alias `~m:<type>:<hostmask>` is accepted and stored in the long form.
Only `external` and `moderated` are supported; other selectors are rejected.
Matching uses the sender's current nick!username@host, not a registered account.
In particular, a nickname-only grant is usable by anyone holding that nickname.
Use a suitable full mask when that distinction matters. Services usernames
retain their complete `+QWKID` suffix and `%HH` escapes in these masks.

Shorthand masks expand to full masks: `Bot` becomes `Bot!*@*`, `user@host`
becomes `*!user@host`, and a bare hostname or IPv6 address becomes `*!*@host`.
Empty components become `*`. Entries are compared without case sensitivity;
adding an existing entry preserves its setter and timestamp. Invalid entries
are rejected rather than truncated. Local changes permit 25 exceptions,
independently of the ban limit. Trusted network state can merge longer lists.

List queries use numerics 348 and 349; invalid arguments use 696. ISUPPORT
advertises `EXCEPTS=e`, `EXTBAN=~,m`, `CHANMODES=be,k,l,imnpst`, and
`MAXLIST=b:25,e:25`. The existing boolean modes remain `imnpst`.

## Persistence

Direct MODE entries have the same lifetime as other channel state. The embedded
ChanServ can retain selected grants across channel recreation and authority
restarts; see [services.md](services.md). It does not automatically import
direct MODE entries into its registration database.

## Linked servers

Servers advertise the optional `CHANEXCEPT` capability. Capable peers exchange
exceptions through MODE, channel bursts, and CSYNC `EXCEPTS` blocks. Authoritative
snapshots reconcile additions and removals while retaining metadata for unchanged
entries. Existing hub authority over untrusted leaves also applies to exceptions.

Peers without this capability remain connected. Their MODE output omits `e`
and its arguments, including within mixed mode changes; bursts omit exceptions.
A legacy snapshot cannot erase an exception list it cannot represent.

Legacy servers do not implement these exemptions and may block messages or joins
that an updated server permits. A legacy hub cannot relay exception state to
updated servers beyond it. Use updated paths for the feature to work throughout
a channel. Existing channel message routing still sends traffic only to server
branches containing channel members; granting an exception creates no new route.
