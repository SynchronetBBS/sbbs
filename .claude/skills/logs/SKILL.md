---
name: logs
description: Use when locating the right Synchronet log file or stream for a given question on Windows or *nix — the per-category files in `data/` (error/hack/spam/guru/hungup/crash/events/sbbsecho/sendmail/logon/csts.tab), the per-day Terminal Server logs in `data/logs/<date>.log`, the Web Server's HTTP access log, the server console (lprintf) stream and its routing to syslog/journalctl/console depending on daemon mode and init system, the sbbsctrl-on-Windows-only `data/logs/{TS,WS,MS,FS}*.LOG` trap, MQTT log subscription, multi-instance traps when sibling BBSes share `text/*.can`, and the `ip.can`/`ip-silent.can` field format. Trigger on "did the auto-filter fire?", "tail the web server log", "where do hack attempts go?", "subscribe to all logs across hosts", "decode an ip.can line", or "why doesn't WS<date>.LOG update on Linux?"
---

# Synchronet log files and streams

**Reference (authoritative):**
- Inventory of log files: https://wiki.synchro.net/monitor:logfiles
- syslog / journalctl on *nix: https://wiki.synchro.net/monitor:syslog
- MQTT topic hierarchy (incl. log subscription): https://wiki.synchro.net/monitor:mqtt
- Log levels and verbosity: https://wiki.synchro.net/monitor:loglevels
- sbbscon command-line flags (daemon modes, syslog facility): https://wiki.synchro.net/monitor:sbbscon
- sbbsctrl (Windows GUI control/monitor): https://wiki.synchro.net/monitor:sbbsctrl
- sbbsNTsvcs (Windows NT Services): https://wiki.synchro.net/monitor:ntsvcs
- Statistics files (separate from log files): https://wiki.synchro.net/monitor:statsfiles
- Multi-pane *nix viewer recipes (tmux + lnav): https://wiki.synchro.net/monitor:multipane
- Interactive term-server monitors: https://wiki.synchro.net/monitor:umonitor (UIFC), https://wiki.synchro.net/monitor:gtkmonitor (GTK)

The wiki has the canonical inventory. **This skill captures the platform-context layer the wiki leaves implicit**: how the *same* Synchronet writes logs differently depending on OS family, init system, daemon/interactive mode, and instance count — and how to choose the right source for a given question.

## The platform axes

Three independent axes determine where the console (lprintf) stream actually lands. The per-category files in `data/` are written the same way on every platform.

| Axis | Settings | What it affects |
|------|----------|-----------------|
| OS family | **Windows** vs ***nix** (Linux, BSD, macOS) | sbbsctrl (Windows GUI) vs sbbscon (*nix terminal); presence of `data/logs/{TS,WS,MS,FS}*.LOG`; backslash paths inside log strings |
| *nix mode | **daemonized** (`sbbs d …`) or `sbbs syslog`, vs **interactive** (`sbbs` with no daemon/syslog flag) | daemonized → syslog; interactive → console/stderr only |
| *nix init | **systemd** vs **everything else** (sysvinit, OpenRC, runit, launchd, no init manager…) | `journalctl -u <unit>` is the natural view on systemd; on other init systems, tail the syslog target file directly (depends on `rsyslog`/`syslog-ng` config — typically `/var/log/syslog` or `/var/log/messages` by distro default, or a dedicated `/var/log/sbbs.log` if the optional `install/rsyslog.d/sbbslog.conf` snippet was deployed) |

The wiki's `monitor:syslog` page lists `tail -f`, `journalctl --follow`, `lnav`, and macOS's `log stream` as the canonical ways to monitor each variant.

## The two output streams (and where each goes)

Every Synchronet server (Terminal, Web, Mail, FTP, Services, plus the BBS event thread) writes log lines through a single `lputs`/`lprintf` channel — the **server console stream** — at a numeric severity (`LOG_EMERG=0` … `LOG_DEBUG=7`; lower number = more severe). Per `monitor:loglevels`, Synchronet doesn't currently emit `LOG_EMERG` or `LOG_ALERT` itself, so the levels you'll see in practice are `CRIT`/`ERR`/`WARNING`/`NOTICE`/`INFO`/`DEBUG`. (`!BLOCKING`, `!CLIENT BLOCKED`, and similar lines with a leading `!` are typically LOG_NOTICE/WARNING — useful as a quick grep target for "something happened that the sysop probably wants to see".) On top of that, several subsystems also append **per-category log files** that capture only domain-specific lines.

```
                    ┌───────────────────────────────────────┐
                    │  lputs(level, "web 0014 HTTP ...")    │  ← every server, every line
                    └────────────────┬──────────────────────┘
                                     │
        ┌────────────────────────────┼─────────────────────────────┐
        ▼                            ▼                             ▼
  *nix interactive:           *nix daemonized              MQTT (if enabled, any OS)
   stdout/stderr              or `sbbs syslog`:            sbbs/+/host/+/server/+/log
  Windows: sbbsctrl GUI       → syslog                     + .../log/<level>
   panes (Term/Web/Mail/Ftp)  → /var/log/* (per rsyslog)   sbbs/+/host/+/event/log
                              → journalctl -u <unit>        sbbs/+/action/<kind>
                                (systemd only)
```

In **parallel**, specific events also append to per-category files in `data/`, regardless of platform:

| File | Written by | What lands here |
|------|-----------|-----------------|
| `data/error.log`     | shared `errorlog()` from any server | LOG_ERR+ lines (anything `!`/unexpected) |
| `data/hack.log`      | shared `hacklog()` from any server | suspected hack attempts (login brute-force, exploit probes); also re-records "!CLIENT BLOCKED" denials at accept |
| `data/spam.log`      | mail server (`spamlog()`) | suspected SPAM delivery attempts |
| `data/guru.log`      | term server | Guru chat sessions |
| `data/hungup.log`    | term server | drops during external programs |
| `data/crash.log`     | term server | activity leading up to suspected node crashes |
| `data/events.log`    | term server event thread | scheduled-event activity |
| `data/sbbsecho.log`  | `sbbsecho` util | FidoNet EchoMail/NetMail processing |
| `data/sendmail.log`  | `exec/sendmail.js` script | **invocations of `sendmail.js`** — a stand-in for `/usr/sbin/sendmail`/`mail`/`mailx` invoked by external programs (mailproc rules, cron jobs, etc.) to inject a message from stdin. **Not** the Mail Server's outbound SMTP — that goes to the mail server's console stream. |
| `data/logon.jsonl`   | `logonlist_lib.js` | structured logon list (consumed by `logonlist` module) |
| `data/logon.lst`     | legacy text logon list | preformatted, consumed by older modules (finger/gopher services, etc.) |
| `data/logs/<date>.log` | term server | per-day user activity (timestamped node-by-node) |
| `data/logs/<date>.lol` | term server | per-day log-off statistics |
| `data/logs/http-*.log` | web server | HTTP access log (one per day, per virtual host); enabled by `HttpLogFile` / formatted by `HttpLogFormat` in `[Web]` |
| `ctrl/csts.tab`      | term server (`logon.cpp`) | cumulative per-day stats — tab-separated ASCII with a header row (`Date / Logons / Timeon / Uploads / UploadB / Dnloads / DnloadB / Posts / Email / Feedback / NewUsers`); readable with the `slog` util / `module:slog`, but also fine to inspect with `cat` / `awk` / a spreadsheet |

The `.log` files **without** a date in the filename rotate automatically — size limit configured in SCFG → System → Advanced → Maximum Log File Size, and for sbbsecho in echocfg.

## *nix specifics: where the console stream actually lands

When `sbbs` runs as a daemon (`sbbs d …` and variants) or interactively with the `syslog` arg, the console stream is routed to syslog with **per-server identifiers** (per `sbbscon.c`):

| Server | syslog ident prefix |
|--------|---------------------|
| Terminal | `term ` |
| Web      | `web  ` |
| Mail     | `mail ` |
| FTP      | `ftp  ` |
| Services | (LOG_DAEMON facility, no extra prefix) |

The full syslog ident is `LogIdent` (from `[UNIX]` in `ctrl/sbbs.ini`, default `synchronet`) concatenated with the server prefix. **On a multi-instance host**, give each BBS instance a distinct `LogIdent` so syslog/journal can separate them — otherwise their streams interleave under one ident and are nearly impossible to disentangle after the fact. (`LogIdent` is independent of the QWK `BBSID` used in MQTT topics; sysops sometimes set `LogIdent` to the BBSID for consistency, but it's not required.)

**Which syslog facility the lines go to** depends on the daemon-mode flag (see `monitor:sbbscon`):

| Invocation | Facility | Typical syslog target file (Debian-family defaults; varies by distro/`rsyslog`/`syslog-ng` config) |
|------------|----------|---------------------------------------------------------------------------------------------------|
| `sbbs d`         | `LOG_USER` (default) | by distro default lands in `/var/log/user.log` (Debian) or `/var/log/messages` (RHEL-family), and the catch-all `/var/log/syslog` on Debian. The repo ships an optional `install/rsyslog.d/sbbslog.conf` snippet that routes Synchronet output to a dedicated `/var/log/sbbs.log` — but it requires `LogFacility=3` (i.e. `sbbs d3`, LOG_LOCAL3) and isn't installed automatically. |
| `sbbs d<X>` (LOCAL0..LOCAL7) | `LOG_LOCAL<X>` | wherever you've routed that LOCAL facility — common pattern for keeping Synchronet output isolated. |
| `sbbs dS` (standard facilities) | **per-server**: term→`LOG_AUTH`, ftp→`LOG_FTP`, mail→`LOG_MAIL`, web/services→`LOG_DAEMON` | each server lands in a *different* file: `auth.log`, `xferlog`, `mail.log`, `daemon.log` respectively. **Easy to miss the web server's lines if you only check `auth.log`** — this is a subtle gotcha. |
| `sbbs syslog` (interactive + syslog) | same as `sbbs d` (LOG_USER) | as above. |

The `LogFacility` value in `[UNIX]` of `ctrl/sbbs.ini` (default `U` = LOG_USER) is what the `d` flag uses; `dS` overrides it with the per-server standard facility.

Rotated copies are typically `.1`, `.2`, … or `.gz` per `logrotate` config. **Watch out for the post-rotation trap**: the current `.log` file is often empty right after rotation; the *live* content is the most recent suffix (`.log.1`, etc.) until the next rotation. Always check timestamps.

**On systemd-managed hosts**, `journalctl -u <unit>` is the better view than grepping multi-GB syslog files — it filters cleanly per service and respects retention/rotation policy:

```
journalctl -u <bbs-unit>           # show all
journalctl --follow -u <web-unit>  # live tail
journalctl --since today -u <web-unit> | grep ...
```

Unit names are **sysop-chosen** — one site might split servers into one unit each (e.g. `synchronet-term.service`, `synchronet-web.service`, `synchronet-mail.service`), another might run everything under a single unit. Discover them with:

```
systemctl list-units --type=service | grep -iE 'sbbs|synchro|httpd|ircd'
systemctl show <unit> -p MainPID -p ExecStart
```

**On non-systemd *nix hosts** (sysvinit, OpenRC, runit, launchd, or just plain `sbbs d …` from a shell), there is no `journalctl`. Tail the syslog file directly, or use `lnav`:

```
tail -f /var/log/syslog            # Debian default (catch-all); /var/log/messages on RHEL-family
tail -f /var/log/sbbs.log          # only if the optional sbbslog.conf snippet is deployed
sudo lnav /var/log/syslog          # interactive log viewer with leveling
log stream --debug --process sbbs  # macOS unified log
```

Find the exact destination on an unfamiliar host by inspecting `/etc/rsyslog.conf` / `/etc/rsyslog.d/`, or by sending a probe message with `logger` and seeing where it appears.

**Interactive (non-daemonized, no `syslog` flag) sbbs** writes to stdout/stderr only — there is *no* syslog/journal capture. That's the right mode for short-lived debugging but it means once you close the terminal, the scrollback is gone (unless you started it under `script`, `tmux`, etc.).

## Windows specifics: front-ends and the `data/logs/{TS,WS,MS,FS}*.LOG` files

Windows has three front-ends that can run Synchronet (see `monitor:sbbscon`, `monitor:sbbsctrl`, `monitor:ntsvcs`):

- **`sbbs.exe`** — the console-mode runner (same code as `sbbscon` on *nix), one Synchronet process with embedded servers; output goes to its console window.
- **`sbbsctrl.exe`** — a GUI control panel hosting the same servers in-process, with one tabbed pane per server.
- **`sbbsNTsvcs.exe`** — runs the servers as Windows NT Services (no GUI; controlled via `services.msc` or sbbsctrl).

The dated `data/logs/{TS,WS,MS,FS}<MMDDYY>.LOG` files are written **only by `sbbsctrl` while it's running** — they're its log-to-disk capture of each pane:

```
data/logs/TS<MMDDYY>.LOG    ← Terminal Server pane
data/logs/WS<MMDDYY>.LOG    ← Web Server pane
data/logs/MS<MMDDYY>.LOG    ← Mail Server pane
data/logs/FS<MMDDYY>.LOG    ← FTP Server pane
```

So **none of the three following situations** writes these files: (a) Windows host running only NT Services without sbbsctrl, (b) Windows host running only `sbbs.exe` console without sbbsctrl, (c) any *nix host. On *nix in particular, **if you find these files they are foreign artifacts** — usually one of:

- left over from a previous Windows install on the same disk/share;
- arriving via shared storage from a Windows sibling host (NAS, SMB share);
- a manual copy from a Windows machine for analysis.

Two dead giveaways that a `WS<date>.LOG` you're reading isn't from the local host: the paths inside use Windows backslashes (e.g. `c:\sbbs\…`, `s:\sbbs\…`), and the timestamps don't correlate to the local host's process restarts. **On *nix, ignore these files and use syslog/journal instead.** On Windows without sbbsctrl, use the Windows Event Log (for NT Services) or the `sbbs.exe` console window directly.

## MQTT: unified log view across hosts and servers

If MQTT is enabled on the BBS (configured in `ctrl/main.ini`'s `[MQTT]` section, see the **`mqtt`** skill for discovery, authentication, and full topic reference), every `lputs` line is also published to the broker — so a single subscriber can see every host's log output, including hosts running different operating systems.

The log-relevant topics (`<BBSID>` is the QWK BBS ID; `<HOSTNAME>` is each instance's configured host name):

```
sbbs/<BBSID>/host/<HOSTNAME>/server/<SERVER>/log[/<N>]   ← server console (<SERVER> ∈ term|mail|ftp|web|srvc), N = level 0–7
sbbs/<BBSID>/host/<HOSTNAME>/event/log[/<N>]              ← event thread
sbbs/<BBSID>/action/{hack,spam,error,login,login_fail,exec}/<KEY>   ← one msg per discrete event
sbbs/<BBSID>/host/<HOSTNAME>/login_attempts/<IP>          ← retained aggregated failed-login snapshot per IP
sbbs/<BBSID>/host/<HOSTNAME>/server/term/max_concurrent/<IP>  ← retained term concurrent-connection strike count per IP
```

Lines published to a bare `…/log` (no level suffix) carry the level in an MQTT v5 user property — receivers can filter without subscribing to all eight per-level subtopics.

For cross-host alerting, the **action** topics are usually more useful than `log` (one message per discrete event, with the subject in the topic itself). For a "current bad-IP state" dashboard, subscribe to the retained `login_attempts/<IP>` snapshots — they replay on connect. For a live feed, subscribe to `action/login_fail/<PROTOCOL>` — but it's an event stream, not retained, so a fresh subscriber sees nothing about pre-connect history. Use both together to get "current state + live updates".

For the `mosquitto_sub` / `mosquitto_pub` invocation, broker discovery, credentials, and TLS — see the `mqtt` skill.

## Multi-instance traps: shared `text/` directories

`text/ip.can` (and other `text/*.can` files) can be written by multiple Synchronet instances when they share a `TextDir`. There are two scenarios that produce the same symptom — an `ip.can` line with no matching creation event in *your* journal — but require different debugging paths:

**Same-host, multiple BBSes.** A single host can run multiple independent Synchronet instances under different `LogIdent`s and `BBSID`s, each with its own `ctrl/` and `data/` trees. They are separate processes — but if both point `TextDir` at the same path, the auto-filter (`filter_ip()` from rate-limit, login-attack, etc.) from either instance appends to the same `ip.can`. The sibling's writes appear under *its* `LogIdent` in the same syslog/journal you're already using — same machine, different unit/ident.

**Cross-host, shared filesystem.** A `text/` directory may live on shared storage (NFS, SMB, GlusterFS, replicated block storage, a sync tool like Syncthing, a `cifs` mount, …) and be mounted by Synchronet instances on physically separate hosts. The auto-filter from any of them appends to the same `ip.can`, but **each host has its own syslog/journal/sbbsctrl** — the creation event for a line on *this* host's `ip.can` may be on a different machine entirely, with its own journal you can't see locally. This is the harder version because there's no local source with the creation line at all.

**How to find the writer when the creation event is missing locally:**

1. **First, check the same-host case.** List every Synchronet instance on this host — `ps -ef | grep -i sbbs`, `systemctl list-units --type=service | grep -iE 'sbbs|synchro|httpd|ircd'`, distinct `LogIdent`s in each instance's `ctrl/sbbs.ini`. Search each one's journal/syslog for the `t=` minute. If found, you're done.
2. **If still missing, check the cross-host case.** Is `text/` on shared storage? `mount | grep text`, `findmnt $SBBS/text`, `stat -f $SBBS/text` will tell you. If yes, the writer is on another host — and the *only* unified view that bridges multiple hosts' log streams is **MQTT**, if it's enabled. Subscribe to `sbbs/+/host/+/server/+/log` and grep the same minute (see the `mqtt` skill). The `<HOSTNAME>` topic component reveals which host fired.
3. **Only then** suspect journald rate-limiting, `SystemMaxRateLimit` drops, or syslog discard. Those do happen, but they're the diagnosis of last resort — almost always the writer is just somewhere you haven't looked yet.

(Reminder: `LogIdent` is the syslog identifier from `[UNIX] LogIdent` in `sbbs.ini`, *not* the QWK `BBSID` used in MQTT topics. The two are independent unless a sysop deliberately sets `LogIdent` to the BBSID — a useful convention but not the default.)

For the converse direction (one BBS deliberately spread across multiple hosts via MQTT, rather than by accidental filesystem sharing), the `sbbs/<BBSID>/host/<HOSTNAME>/...` hierarchy keeps each host's stream cleanly separable — that's the same hierarchy that lets you find the writer in the cross-host case above.

## ip.can / ip-silent.can: format of auto-filter writes

Both are tab-delimited; one filter entry per line. The web server's rate-limit filter, the login-attempt filter, and `filter_ip()` callers in JS all write the same format:

```
<target>  t=<iso-timestamp>  p=<protocol>  r=<reason>  e=<iso-expiry>  [h=<hostname>]  [u=<user>]
```

`<target>` is either a bare IP (e.g. `192.0.2.10`) or a CIDR (e.g. `198.51.100.0/24`). Fields:

| Field | Meaning |
|-------|---------|
| `t=` | when the filter was added (ISO 8601 with offset) |
| `p=` | originating protocol (`HTTP`, `HTTPS`, `Telnet`, `SSH`, `FTP`, `SMTP`, …) |
| `r=` | human-readable reason; for rate-limit auto-filters this is `"<N> rate-limit violations"` for `/32` host blocks and `"<N> rate-limit violations from <M> IPs"` for subnet blocks |
| `e=` | expiry (matches `rate_limit_filter_duration` etc.); absent = permanent |
| `h=` | hostname at filter time, when known |
| `u=` | offending username, when applicable |

The matching console-log creation line (`!BLOCKING IP ADDRESS: …` or `!BLOCKING SUBNET: …`) is the audit trail showing **which server** and **which instance** put the entry there — always cross-check the syslog ident before assuming it was yours.

`ip-silent.can` is identical in format but is consulted at `accept()` time and drops the connection without sending a response (set via `RateLimitFilterSilent=true`).

## Quick map: "what should I tail for X?"

| Question | First place to look |
|----------|---------------------|
| Did the web rate-limit filter fire? | console stream (syslog/journal/sbbsctrl pane) → grep `!BLOCKING` and `rate-limit`; then `text/ip.can` for the resulting entry |
| Is there active rate-limit deny pressure right now? | live tail the web server's console stream → grep `Too many requests` |
| Failed login attempts (any server) | `data/hack.log`; MQTT `action/login_fail/<PROT>`; MQTT retained `host/+/login_attempts/+` |
| Why did a node drop / hang during an external? | `data/hungup.log`, `data/crash.log` |
| Is a daily event running / mis-running? | `data/events.log` (term server only — no events on a web-/mail-only host) |
| FidoNet packet flow | `data/sbbsecho.log` |
| `sendmail.js` invocations (cron / external mail injection) | `data/sendmail.log` |
| Mail Server outbound SMTP / relay activity | mail server's console stream — **not** `sendmail.log` |
| HTTP request log (URLs, response codes) | `data/logs/http-<host>-YYYY-MM-DD.log` (only if `HttpLogFile` is set) |
| User activity for a date | `data/logs/<MMDDYY>.log` (term server) |
| Cross-host real-time view | MQTT subscribe `sbbs/+/host/+/server/+/log` and `sbbs/+/action/#` |
| Console scrollback (*nix, systemd, days old) | `journalctl --since '<date>' -u <unit>` |
| Console scrollback (*nix, systemd, live tail) | `journalctl -fu <unit>` |
| Console scrollback (*nix, non-systemd) | `tail -f /var/log/syslog` (Debian catch-all) / `/var/log/messages` (RHEL-family) / `/var/log/sbbs.log` (if optional `sbbslog.conf` snippet was deployed with `LogFacility=3`); `lnav` for interactive |
| Console scrollback (*nix, interactive sbbs, no syslog flag) | only what's still in your terminal scrollback — start under `tmux`/`script` next time |
| Console scrollback (Windows) | sbbsctrl panes, or `data/logs/{TS,WS,MS,FS}<MMDDYY>.LOG` — NOT applicable on *nix |
| Suspected SPAM delivery | `data/spam.log` |
| Cumulative stats (logons, calls, etc.) | `ctrl/csts.tab` (tab-separated ASCII; `slog` util / `module:slog` print it, but `cat` / `awk` / a spreadsheet work too). Other stats (not logs) — `ctrl/dsts.ini`, `data/echostats.ini`, `data/binkstats.ini` — are documented at `monitor:statsfiles`. |

## Common mistakes

- **Treating `data/logs/WS<MMDDYY>.LOG` (or `TS`/`MS`/`FS`) as the *nix web/term/mail/ftp log.** They aren't — they're sbbsctrl-on-Windows output. On *nix, syslog/journal is the source. Backslashed paths inside the file (e.g. `c:\sbbs\…`, `s:\sbbs\…`) confirm a Windows origin.
- **Reading the current `.log` file and concluding nothing happened.** If your host rotates syslog (logrotate, etc.), the live `.log` is often the empty post-rotation head — the actual recent content is in `.log.1` (or `.log.1.gz`). Check timestamps before grepping just one file.
- **Assuming an `ip.can` entry was created by *your* instance.** On a host with multiple BBSes sharing a `text/` directory, a sibling's web/term server may have written the line. Match the entry's `t=` timestamp against each instance's console stream before blaming code.
- **Grepping the wrong systemd unit for "the web server".** A site that runs the web server as its own daemon (`sbbs d w!`) typically puts it under a separate systemd unit from the BBS — its log lines won't appear under the BBS unit's journal. Discover the right unit with `systemctl list-units` and `systemctl show <unit> -p ExecStart`.
- **Mistaking `sendmail.log` for the Mail Server's outbound log.** It only records invocations of `exec/sendmail.js` (the `/usr/sbin/sendmail`-replacement). The Mail Server's own SMTP send/relay activity is in its console stream; SMTP delivery problems are summarised in `data/error.log`.
- **Forgetting that `HttpLogFile=` (empty) disables `data/logs/http-*.log`.** No HTTP access log means reconstructing request flow from the web server's console stream — slower, but available.
- **Assuming the term server is running on a host that only serves web/mail.** No term server → no `events.log`, no `data/logs/<date>.log`, no `crash.log`, no `csts.tab` updates.
- **Confusing `action/login_fail` (event stream, not retained) with `login_attempts/<IP>` (retained snapshot).** Subscribing only to the retained topic on first connect gives you the *current* state; subscribing only to the event topic gives you new failures going forward but nothing about pre-connect history. Subscribe to both for "current state + live updates".
- **Tailing an interactive (non-daemonized) sbbs without preserving its console.** No syslog, no journal — close the terminal and the log is gone. Run under `tmux`/`screen`/`script` if you need to keep it.

## Configuration knobs worth knowing

- `[UNIX] LogIdent` (default `synchronet`) and `LogFacility` (default `U` = `LOG_USER`) in `ctrl/sbbs.ini` — controls the syslog target. **Set a distinct `LogIdent` per BBS instance on a multi-instance host** so syslog/journal can separate them.
- `[Web] HttpLogFile` and `HttpLogFormat` (NCSA-style) — enables and formats `data/logs/http-*.log`.
- SCFG → System → Advanced → Maximum Log File Size — rotation threshold for the un-dated `.log` files in `data/`.
- SCFG → Networks → MQTT → Publish Verbosity — `High` makes per-server status, per-node status, and full log streams flow; `Low` only publishes terminal status. Default is conservative.
- Log level (severity floor for what gets emitted) is set per server in SCFG → Servers → \<server\> → Log Level (and per-network in echocfg). See the loglevels wiki page.

## Investigation cookbook

**"Did the rate-limit auto-filter actually fire today?"**

```
# *nix, systemd:
journalctl -u <web-unit> --since today | grep -E '!BLOCKING|rate-limit violations'

# *nix, non-systemd (path depends on rsyslog config: /var/log/syslog, /var/log/messages, or /var/log/sbbs.log):
grep -E '!BLOCKING|rate-limit violations' /var/log/<your-syslog-target>*

# Then cross-check the resulting ip.can entries:
awk -F'\t' '/rate-limit/ {print}' $SBBS/text/ip.can
```

`!BLOCKING IP ADDRESS:` (or `!BLOCKING SUBNET:`) is the creation event — one per filter. The `!CLIENT BLOCKED in …/ip.can since …` lines that follow are *repeated* deny-at-accept hits — noisy but harmless; they confirm the filter is still suppressing the source.

**If you find a recent `text/ip.can` entry but the matching `!BLOCKING` creation line is missing from your journal**, don't jump to "journald lost it" or "syslog rate-limited the message". Almost always it means another Synchronet instance — either a sibling on this host, or one on another host that shares the same `text/` directory — wrote the entry. See [Multi-instance traps](#multi-instance-traps-shared-text-directories). Rule out the multi-instance case before suspecting log infrastructure drops.

**"Who's hammering the BBS right now (any server)?"**

```
# *nix, systemd:
journalctl --since '-5 minutes' -u <bbs-unit> -u <web-unit> \
  | grep -E '!|denied|limit|BLOCKED' | head

# Any platform, MQTT-equipped:
mosquitto_sub -h <broker> -v \
  -t 'sbbs/+/action/login_fail/#' \
  -t 'sbbs/+/action/hack/#'
```

**"This `text/ip.can` entry didn't come from my instance — who wrote it?"**

```
# Take the t= timestamp from the offending line, then look in every sibling
# instance's console stream for that minute:

# *nix, systemd:
journalctl --since '<YYYY-MM-DD HH:MM:30>' --until '<YYYY-MM-DD HH:MM+1:30>' \
  -u <unit-a> -u <unit-b> ...

# *nix, non-systemd (substitute your actual syslog target):
grep -h '<YYYY-MM-DDTHH:MM>' /var/log/<your-syslog-target>*
```

The matching `!BLOCKING IP ADDRESS: <that-ip>` line carries the writer's `LogIdent` in its syslog identifier — that tells you which instance fired it.
