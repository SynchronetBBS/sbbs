---
name: mqtt
description: Use when working with Synchronet's MQTT integration — discovering whether MQTT is enabled (the `[MQTT]` section of `ctrl/main.ini`, **not** `sbbs.ini`), reading broker address/port/credentials/TLS settings, connecting with `mosquitto_sub`/`mosquitto_pub` (anonymous, user+pass, or one of four TLS modes: CA, mTLS, PSK, SBBS-internal-CA), or interacting with the topic tree for **monitoring** (server/node status, node-output spying, `action/#`, retained `login_attempts/<IP>` and `max_concurrent/<IP>`, log streams `…/log[/<N>]`) or **controlling** the BBS (production-impacting topics that recycle/pause/resume servers, clear failed-login lists, set node flags, inject input or messages into a node — plus the local-filesystem equivalents in `ctrl/`). Trigger on "is MQTT enabled", "what's the broker address", "subscribe to all sbbs logs", "pause/recycle via MQTT", "inject a message into node N", "clear failed-login for an IP", or "spy on a node remotely".
---

# Synchronet MQTT — monitor and control

**Reference (authoritative):**
- MQTT topic hierarchy and semantics: https://wiki.synchro.net/monitor:mqtt
- SCFG configuration UI for MQTT: SCFG → Networks → MQTT
- Log levels (used for `…/log/<N>` per-level subtopics): https://wiki.synchro.net/monitor:loglevels
- Related — file/console log streams: see the `logs` skill.

**MQTT in Synchronet is two-way.** Most use cases are read-only monitoring, but the same broker also carries **control-plane** topics that recycle servers, pause/resume listeners, clear filter lists, set node flags, and inject input. Read the [Control plane — production-impacting operations](#control-plane--production-impacting-operations) section before publishing anything.

## Step 1 — discover whether MQTT is enabled and how to connect

The `[MQTT]` section lives in **`ctrl/main.ini`**, not `ctrl/sbbs.ini`. (This trips a lot of people; the rest of Synchronet's server/daemon config is in `sbbs.ini`, but MQTT belongs to BBS-level system settings in `main.ini`.) Read it:

```
grep -A 20 '^\[MQTT\]' <sbbs>/ctrl/main.ini
```

Keys (defaults shown; full source of truth is `scfglib1.c::read_main_cfg`):

| Key | Default | Meaning |
|-----|---------|---------|
| `Enabled` | `false` | master switch; if `false`, no topics are published or accepted |
| `InternalBroker` | `false` | if `true`, Synchronet runs its own embedded broker (no external mosquitto needed); otherwise connect to the address below |
| `Broker_addr` | `127.0.0.1` | IPv4/IPv6/hostname of the MQTT broker |
| `Broker_port` | `1883` (`IPPORT_MQTT`) | broker TCP port (typically `1883` plain, `8883` TLS) |
| `Username` | empty | MQTT username; empty = anonymous |
| `Password` | empty | MQTT password; **plain text in the file** — restrict file permissions |
| `Keepalive` | `60` (seconds; range 5–INT_MAX) | client keepalive interval |
| `Protocol_version` | `4` (range 3–5; 3=v3.1, 4=v3.1.1, 5=v5) | MQTT protocol version. v5 unlocks per-message user properties (Synchronet uses these for log-level tagging on `…/log` without level suffix). |
| `Publish_QOS` | `0` (range 0–2) | QoS for messages Synchronet *publishes* |
| `Subscribe_QOS` | `2` (range 0–2) | QoS for subscriptions Synchronet *places* |
| `LogLevel` | `Info` | minimum severity of console lines republished to MQTT |
| `Verbose` | `true` | publish per-node status, per-server status, and full log streams. `false` reduces to terminal status only. (Mirrors SCFG's "Publish Verbosity = High" vs "Low".) |
| `TLS_mode` | `0` (disabled) | see TLS table below |
| `TLS_cafile` | empty | path to a CA cert/bundle (used by modes 1, 2) |
| `TLS_certfile` | empty | path to client certificate (mode 2) |
| `TLS_keyfile` | empty | path to client private key (mode 2) |
| `TLS_keypass` | empty | passphrase for the client key, if encrypted |
| `TLS_psk` | empty | pre-shared key, hex-encoded (mode 3) |
| `TLS_identity` | empty | PSK identity string (mode 3) |

**`TLS_mode` enumeration** (from `mqtt.h`):

| Value | Name | What it means | What you need |
|-------|------|---------------|---------------|
| `0` | `MQTT_TLS_DISABLED` | plain TCP | nothing |
| `1` | `MQTT_TLS_CA` | TLS, broker cert verified against a CA bundle | `TLS_cafile` |
| `2` | `MQTT_TLS_CERT` | mTLS — client also presents a cert | `TLS_cafile`, `TLS_certfile`, `TLS_keyfile` (+ `TLS_keypass` if encrypted) |
| `3` | `MQTT_TLS_PSK` | pre-shared-key TLS | `TLS_psk` (hex), `TLS_identity` |
| `4` | `MQTT_TLS_SBBS` | use Synchronet's own internal CA and the BBS's web/server cert | nothing extra; the BBS handles it |

**Security note**: `ctrl/main.ini` contains `Password=` and possibly `TLS_keypass=` **in plain text**. The default install on a typical Linux host is world-readable; tighten with `chmod 600 ctrl/main.ini` (and chown to the BBS user) if your threat model includes other local users.

## Step 2 — connect with mosquitto_sub / mosquitto_pub

Once you know broker/port/auth/TLS from `main.ini`, plain-TCP (`TLS_mode=0`) anonymous:

```
mosquitto_sub -h <broker> -p <port> -v -t 'sbbs/#'
```

With username/password:

```
mosquitto_sub -h <broker> -p <port> -u <user> -P <pass> -v -t 'sbbs/#'
```

With TLS mode 1 (CA-verified broker, anonymous):

```
mosquitto_sub -h <broker> -p <port> --cafile <ca-bundle> -v -t 'sbbs/#'
```

With TLS mode 2 (mTLS):

```
mosquitto_sub -h <broker> -p <port> \
  --cafile <ca> --cert <client-cert> --key <client-key> \
  -v -t 'sbbs/#'
```

With TLS mode 3 (PSK):

```
mosquitto_sub -h <broker> -p <port> --psk <hex-key> --psk-identity <identity> \
  -v -t 'sbbs/#'
```

TLS mode 4 (`MQTT_TLS_SBBS`) uses the BBS's own CA + cert internally — an *external* client (mosquitto_sub) doesn't get that bundle automatically; you'd need the broker's CA file out-of-band to verify it, or just use `--insecure` for quick diagnostics (skips hostname verification — fine for a local test broker, **not** for production).

**MQTT protocol version** — `mosquitto_sub`/`-pub` default to v3.1.1 (4). Match the broker's expectation; for v5 add `-V 5`. Note that the level-as-user-property convention on Synchronet's bare `…/log` topics only works on v5.

## Topic hierarchy at a glance

All topics start with `sbbs/<BBSID>` where `<BBSID>` is the QWK BBS ID (SCFG → Message Options). Under that:

```
sbbs/<BBSID>                            ← BBS name (retained payload)
sbbs/<BBSID>/node                       ← total node count
sbbs/<BBSID>/node/<N>                   ← human-readable status (verbose mode)
sbbs/<BBSID>/node/<N>/status            ← tab-delimited node status fields (see exec/load/nodedefs.js)
sbbs/<BBSID>/node/<N>/terminal          ← tab-delimited last-connected terminal (cols, rows, syncterm, ANSI, CP437, …)
sbbs/<BBSID>/node/<N>/output            ← live terminal output (for spying)
sbbs/<BBSID>/action/<KIND>/<KEY>        ← BBS-wide client actions (see Action topics below)
sbbs/<BBSID>/host/<HOSTNAME>            ← public host name (retained)
sbbs/<BBSID>/host/<HOSTNAME>/login_attempts/<IP>  ← retained failed-login aggregate per IP
sbbs/<BBSID>/host/<HOSTNAME>/server/<SERVER>      ← server status line (state + counters)
sbbs/<BBSID>/host/<HOSTNAME>/server/<SERVER>/log      ← console log (level in v5 user property)
sbbs/<BBSID>/host/<HOSTNAME>/server/<SERVER>/log/<N>  ← console log filtered to level N
sbbs/<BBSID>/host/<HOSTNAME>/server/<SERVER>/client/action/<KIND>  ← per-server client events
sbbs/<BBSID>/host/<HOSTNAME>/server/<SERVER>/error_count           ← total errors since server start (retained)
sbbs/<BBSID>/host/<HOSTNAME>/server/<SERVER>/max_concurrent/<IP>   ← retained term concurrent-connection strikes
sbbs/<BBSID>/host/<HOSTNAME>/event/log[/<N>]      ← event-thread log
```

`<SERVER>` is one of: `term` (Terminal), `mail` (Mail), `ftp` (FTP), `web` (Web), `srvc` (Services).

### Action topics (BBS-wide client events, non-retained)

```
sbbs/<BBSID>/action/hack/<METHOD>       ← suspected hack attempt
sbbs/<BBSID>/action/spam/<ACTION>       ← suspected SPAM
sbbs/<BBSID>/action/error/<LEVEL>       ← unexpected condition
sbbs/<BBSID>/action/exec/<PROGCODE>     ← external program executed
sbbs/<BBSID>/action/login/<PROTOCOL>    ← successful auth
sbbs/<BBSID>/action/login_fail/<PROTOCOL>   ← failed auth
```

These fire **one message per event**, not retained — a fresh subscriber sees only events from connect-time forward. Pair with the retained `login_attempts/<IP>` snapshot for "current state + live updates".

### Server state strings

`sbbs/<BBSID>/host/<HOSTNAME>/server/<SERVER>` carries a status string whose *first field* is one of:

| State | Meaning |
|-------|---------|
| `stopped`       | server not running |
| `initializing`  | starting up |
| `ready`         | accepting clients |
| `paused`        | running but rejecting new connections (see `pause` control) |
| `reloading`     | re-reading config |
| `stopping`      | shutting down |
| `disconnected`  | MQTT client lost the broker; published from a will-message |

## Retention semantics

Most Synchronet MQTT messages are published **retained**. Retained messages persist on the broker until either the broker restarts (without persistence configured) or someone publishes an empty payload to the same topic to clear it. **Consequence**: a fresh `mosquitto_sub` connection replays the latest retained value of every matched topic immediately — useful for dashboards, surprising if you expect an event stream.

The non-retained topics are: `action/#`, `node/+/output`, and the various `log` topics (those flow live).

## Control plane — production-impacting operations

**Anything below this line affects users connected to the BBS in real time.** Read the warnings before publishing.

Synchronet listens for control messages on the same broker. Most accept any payload (empty is fine) — they trigger a flag rather than passing data. Where payload matters it's noted.

### Host-wide controls (affect every server on a host)

```
sbbs/<BBSID>/host/<HOSTNAME>/recycle     ← restart ALL servers on that host
sbbs/<BBSID>/host/<HOSTNAME>/pause       ← stop accepting new connections on ALL servers
sbbs/<BBSID>/host/<HOSTNAME>/resume      ← undo pause
sbbs/<BBSID>/host/<HOSTNAME>/clear       ← clear failed-login attempt list + term max-concurrent strike list
                                            payload: empty = clear everything; "<IP>" = clear only that IP
```

`recycle` will drop in-flight connections during the restart window. `pause` keeps existing users connected but new logins fail until `resume`. **Don't publish to these unless you mean it** — there's no confirmation prompt.

The same effects can be triggered locally (no MQTT needed) by creating a flag file in `ctrl/`:

| File | Equivalent topic |
|------|------------------|
| `ctrl/recycle` (or `recycle.term`, `recycle.web`, …) | `host/+/recycle` (or `server/+/recycle`) |
| `ctrl/clear` (or `clear.term`, `clear.ftp`, …) | `host/+/clear` (or `server/+/clear`) |

The flag file's first line is read as an optional IP filter (for `clear`); empty content means "clear all". The file is removed by Synchronet once consumed.

### Per-server controls

```
sbbs/<BBSID>/host/<HOSTNAME>/server/<SERVER>/clear   ← clear that server's view of the failed-login list
                                                       (and, for term, max-concurrent strikes)
                                                       payload: empty or "<IP>"
```

Per-server `recycle` / `pause` / `resume` follow the same pattern (one server only, instead of all).

### Node controls (Terminal Server)

```
sbbs/<BBSID>/node/<N>/input             ← inject keyboard input into the connected user's session (payload = keys to send)
sbbs/<BBSID>/node/<N>/msg               ← send a short text message to the node (payload = text, terminate with \n)
sbbs/<BBSID>/node/<N>/set/status        ← set node status integer; "0"=waiting, "5"=offline
sbbs/<BBSID>/node/<N>/set/errors        ← set error counter ("0" to clear)
sbbs/<BBSID>/node/<N>/set/misc          ← set misc attribute flags; integer ("0x"-prefixed = hex)
sbbs/<BBSID>/node/<N>/set/lock          ← only sysop can login; "0" to clear
sbbs/<BBSID>/node/<N>/set/intr          ← interrupt (disconnect) the user; "0" to clear
sbbs/<BBSID>/node/<N>/set/down          ← mark node unavailable (status = 5); "0" to clear
sbbs/<BBSID>/node/<N>/set/rerun         ← reload config on next connection; "0" to clear
```

`input` is essentially remote keystrokes — anything you send is interpreted by the user's current screen as if typed. `intr` disconnects the user immediately. `down` prevents anyone from getting that node until cleared. **All four affect the live user; use with care.**

### Safety patterns

- **Subscribe before you publish.** Run `mosquitto_sub -v -t 'sbbs/+/host/+/server/+/log' -t 'sbbs/+/action/#' -t 'sbbs/+/host/+/server/+'` in another terminal so you can see what your control message *did*.
- **Prefer the narrowest scope.** A per-server `recycle` is less disruptive than a host-wide one; a single-IP `clear/<IP>` is much less than a wholesale `clear`.
- **Test with non-destructive ops first.** Status snapshots (retained) and `error_count` reads are free of side effects; use them to confirm the topics work before issuing a `pause` or `intr`.
- **Beware retained control topics.** If a sysop has accidentally retained a `pause` message in the past, the next time the BBS reconnects to the broker it could replay the retained message and re-pause itself. Clear stale retained control messages by publishing an empty payload to the same topic with `-r`.

## Recipes

**"Is MQTT enabled on this host, and where's the broker?"**

```
awk '/^\[MQTT\]/,/^\[/' <sbbs>/ctrl/main.ini
```

If `Enabled=false` (or the section is missing), no further work is possible until SCFG → Networks → MQTT is set up. If `InternalBroker=true`, the broker is Synchronet itself (default port `Broker_port`); otherwise connect to `Broker_addr:Broker_port`.

**"Subscribe to every log line across every host on the broker."**

```
mosquitto_sub -h <broker> -p <port> -V 5 -v \
  -t 'sbbs/+/host/+/server/+/log' \
  -t 'sbbs/+/host/+/event/log'
```

(`-V 5` so you get the level user-property on the bare `…/log` topic.)

**"What's happening across the BBS right now — actions only, terse."**

```
mosquitto_sub -h <broker> -p <port> -v -t 'sbbs/+/action/#'
```

**"Show the current per-IP failed-login state (retained dashboard)."**

```
mosquitto_sub -h <broker> -p <port> -v -t 'sbbs/+/host/+/login_attempts/+'
```

A fresh subscriber sees every tracked IP's last-attempt payload immediately (retained replay), then updates live.

**"Spy on what node 3 is sending to the user (remote screen capture)."**

```
mosquitto_sub -h <broker> -p <port> -t 'sbbs/<BBSID>/node/3/output'
```

The payload is the terminal byte stream (CP437 by default, including ANSI control sequences) — pipe through a terminal-capable viewer if you want it rendered.

**"Clear a single offending IP from the failed-login list, host-wide."**

```
mosquitto_pub -h <broker> -p <port> -t 'sbbs/<BBSID>/host/<HOSTNAME>/clear' -m '203.0.113.42'
```

(Or, locally: `echo 203.0.113.42 > <sbbs>/ctrl/clear`.)

**"Recycle just the web server on host X."**

```
mosquitto_pub -h <broker> -p <port> -t 'sbbs/<BBSID>/host/<HOSTNAME>/server/web/recycle' -m ''
```

Verify the result by watching the matching `…/server/web` status topic — you should see `stopping` → `initializing` → `ready` within seconds.

## Common mistakes

- **Looking for `[MQTT]` in `sbbs.ini`.** It's in `main.ini`. `sbbs.ini` has the per-server (term/web/mail/ftp/services) settings; MQTT is a BBS-level concern and lives with the rest of the system config in `main.ini`.
- **Forgetting `-V 5`.** Without it, `mosquitto_sub` defaults to v3.1.1 and you don't see the MQTT v5 user property carrying the log level on bare `…/log` topics — you'd have to subscribe to all eight `…/log/<N>` subtopics instead.
- **Reading the password from `main.ini` and pasting it into a shell history.** Use `-P "$(awk -F= '/^[[:space:]]*Password=/{print $2}' <sbbs>/ctrl/main.ini)"` or set `MOSQUITTO_PWD` and read with `-P "$MOSQUITTO_PWD"`. Better still, configure a dedicated read-only MQTT user for diagnostics.
- **Treating action topics as retained.** They aren't — a fresh subscriber sees only new events. For pre-connect state, use the retained `login_attempts/<IP>` and `max_concurrent/<IP>` topics in parallel.
- **Publishing to a control topic without `-r ''` cleanup.** If you publish a one-shot control with `-r` set (retained), Synchronet may re-consume it on reconnect; clear by publishing an empty retained payload to the same topic afterwards.
- **Subscribing to `node/+/output` and dumping straight to a terminal.** The payload contains ANSI CSI sequences that may reposition your cursor, clear your screen, change colours, etc. Pipe to a file or a controlled viewer for analysis.
- **Forgetting `Verbose=true`.** In `false`/"Low" mode, per-server status and full log streams are suppressed; you'll only see terminal node status. Bump it in SCFG → Networks → MQTT → Publish Verbosity if you need the rest.
- **Sending a `node/+/msg` without a trailing newline.** Per the wiki, the message payload must end with `\n` to be delivered cleanly; use `-m $'<text>\n'` in bash.
- **Using TLS mode 4 (`MQTT_TLS_SBBS`) without the BBS's CA out-of-band.** That mode uses Synchronet's internal CA and the BBS's own cert; an external client can't validate it without the CA file. For local diagnostics use `--insecure`; for production, distribute the CA properly or use TLS mode 1.

## Cross-references

- For the file/console log streams (per-category files in `data/`, syslog/journalctl on *nix, the `WS*.LOG` sbbsctrl-on-Windows trap, and the `ip.can` field format) — see the **`logs`** skill.
- For low-level config parsing in `scfglib1.c` (the source of truth for the `[MQTT]` defaults and ranges quoted here) — search for `read_main_cfg` in `src/sbbs3/scfglib1.c`.
- For MQTT-related JS host bindings (`MQTT` object, `mqtt.publish()`, etc.) — see the **`javascript`** skill.
