---
name: synchronet-control
description: Use when controlling a running Synchronet instance — recycling/reloading, shutdown, pause, clearing the failed-login list, or forcing a timed event. Covers cross-platform **semaphore files** in `ctrl/`/`data/` (`recycle`/`shutdown`/`pause`/`clear` with per-server `.<service>` and per-host `.<hostname>` suffix variants for shared-`ctrl/` setups), POSIX signals to `sbbscon` (SIGTERM/etc. = quit, SIGHUP = recycle-all), OS service managers (systemctl, service, launchctl, sc.exe), Windows front-ends (sbbsctrl, sbbsNTsvcs), the `node rerun` utility, the "won't recycle while in use" gotcha, and the `NO_RECYCLE` flag. For control via MQTT — often preferred even on the same host — see the `synchronet-mqtt` skill. Trigger on "recycle the web server", "restart sbbs", "pause connections", "clear failed-login list", "send a signal to sbbs", "force a timed event", or "remove an IP ban".
---

# Synchronet — controlling a running instance

**Reference (authoritative):**
- Semaphore files inventory: https://wiki.synchro.net/config:semfiles
- sbbscon command-line / signal flags: https://wiki.synchro.net/monitor:sbbscon
- sbbsctrl (Windows GUI): https://wiki.synchro.net/monitor:sbbsctrl
- sbbsNTsvcs (Windows NT Services): https://wiki.synchro.net/monitor:ntsvcs
- "Why doesn't my recycle take effect?" FAQ: https://wiki.synchro.net/faq:nix#recycle
- `node` control utility: https://wiki.synchro.net/util:node
- For **remote** / cross-host control via MQTT: see the `synchronet-mqtt` skill.

The skill captures **which lever is right for which job**. Synchronet's own semaphore-file mechanism is the cross-platform native primitive — it works the same on Linux, BSD, macOS, and Windows, with or without systemd / sbbsctrl / NT Services in front. OS-level service managers and signals are the escalation path when the semfile isn't enough.

## Choose the gentlest lever that does the job

```
   Less disruptive ─────────────────────────────────────────────► More disruptive
   ┌────────────┬────────────┬────────────┬────────────┬────────────┬────────────┐
   │ touch      │ touch      │ touch      │ kill -HUP  │ systemctl  │ kill -9 /  │
   │ ctrl/clear │ ctrl/pause │ ctrl/recyc │ <pid>      │ stop+start │ TerminateP │
   │ (or .S)    │ (or .S)    │ le (or .S) │ (recycle-  │ / sc stop  │ rocess     │
   │            │            │ - .ini     │  all)      │  / sc      │            │
   │            │            │   reload   │            │  start     │            │
   │            │            │ - threads  │            │            │            │
   │            │            │   restart  │            │            │            │
   └────────────┴────────────┴────────────┴────────────┴────────────┴────────────┘
   (in-process    (existing    (full server  (every server  (process     (data-loss
    list clear)   sessions     reinit;       in one         restart;     risk; only
                  remain)      no process    handler;       any open     for hung
                               restart)      no process     sessions     processes)
                                             restart)      drop)
```

The skill below documents each rung. **Default to the gentlest one that achieves the goal**, and never reach for `kill -9` while a less destructive option is untried.

## Synchronet semaphore files (cross-platform primitive)

Synchronet servers poll for semaphore files in `ctrl/` (mostly) or `data/` (events). When the **mtime** of a known semfile path advances, the corresponding action fires on the next poll tick (`SemaphoreCheckFrequency`, typically a couple of seconds). Contents are mostly ignored — *touching* the file is the signal. The exception is `clear`, whose first line is read as an optional IP filter.

*(Each operation below has an MQTT equivalent that's preferable in several situations — see [When MQTT is preferred](#when-mqtt-is-preferred-even-on-the-same-host) below. The semfile path is documented first because it's the always-available, cross-platform primitive; choose between them based on the criteria in that section.)*

### The four lifecycle actions

| Action | File (global) | What happens |
|--------|---------------|--------------|
| `recycle` | `ctrl/recycle` | Server terminates its threads, closes sockets, re-reads its INI(s), and restarts — **same process, new state**. No client of *that* server stays connected. Other servers untouched (unless the global file fires them all). |
| `shutdown` | `ctrl/shutdown` | Server exits permanently — same process keeps running for *other* servers but this one is gone until the process restarts. |
| `pause` | `ctrl/pause` | Server stops accepting *new* connections; existing sessions continue. State becomes `paused`. **Removing the file resumes.** (Note: `pause` uses file existence, not mtime — delete to resume.) |
| `clear` | `ctrl/clear` | Clears the in-memory failed-login attempt list (and, for the term server, the max-concurrent-connection strike list). First non-empty line of the file is read as an optional IP — present → clear only that IP, absent → clear all. |

### Targeting: scope by suffix

For any action `<A>`, Synchronet polls the following paths in `ctrl/` (verified against `xpdev/semfile.c::semfile_list_init`):

| Path | Scope |
|------|-------|
| `ctrl/<A>`                              | **every** server on **every** host |
| `ctrl/<A>.<service>`                    | one server: `<service>` ∈ `term`, `web`, `mail`, `ftp`, `services`. (The `services` server's own polled abbrev is the long form `services`, not `srvc`; per-server config uses `srvc` internally. Use `.services` for the semfile.) |
| `ctrl/<A>.<hostname>`                   | every server **on this host only** (matched against `gethostname()`'s FQDN) |
| `ctrl/<A>.<hostname>.<service>`         | one server on this host only |
| `ctrl/<A>.<short-hostname>`             | as above but matched against the short hostname (first dot-separated component) |
| `ctrl/<A>.<short-hostname>.<service>`   | short hostname + one server |

**The hostname variants are essential when `ctrl/` is on shared storage** (NFS/SMB/CIFS/Syncthing) and you only want one host to react. Plain `ctrl/recycle` would otherwise fire on every host that mounts the share. (See the `synchronet-logs` skill's "Multi-instance traps" section for the symptom on the log side.)

Examples:

```
touch  ctrl/recycle                          # recycle every server on every host
touch  ctrl/recycle.web                      # recycle just the web server, all hosts
touch  ctrl/recycle.web1.example.com         # recycle every server only on host web1.example.com
touch  ctrl/recycle.web1.example.com.web     # recycle just the web server on host web1
echo 203.0.113.5 > ctrl/clear.term           # clear that one IP from the term server's fail list
echo                > ctrl/clear             # (empty) clear every server's fail list everywhere
```

### Special recycle triggers (config-file watches)

Each server's polling loop also includes its INI file in the recycle list — so editing and saving the config implicitly recycles the server:

| Server | Watched INI(s) | Source |
|--------|----------------|--------|
| Terminal | `ctrl/sbbs.ini`, `ctrl/text.dat`, `ctrl/text.ini`, `ctrl/attr.ini` | `main.cpp` |
| Web | `ctrl/sbbs.ini` | `websrvr.cpp` |
| Mail | `ctrl/sbbs.ini` | `mailsrvr.cpp` |
| FTP | `ctrl/sbbs.ini` | `ftpsrvr.cpp` |
| Services | `ctrl/sbbs.ini`, `ctrl/services.ini` | `services.cpp` |

Touching `ctrl/sbbs.ini` therefore recycles every server. That's intentional — sysop edits (SCFG saves, `text.dat` regen via `textgen`, etc.) become live without an explicit `touch ctrl/recycle`. **Side effect**: a backup tool, `rsync` without `-t`/`-a`, or any utility that touches mtime can accidentally trigger a recycle; tell backups to preserve mtime, or filter `ctrl/sbbs.ini`/`ctrl/text.dat`/`ctrl/*.ini` out of any mtime-modifying sync.

### Event-trigger semfiles in `data/`

A separate category — these don't restart servers, they fire timed events:

| File | Effect |
|------|--------|
| `data/<event>.now` | Force the event with internal code `<event>` (SCFG → External Programs → Timed Events) to run on the term server's event thread |
| `data/prepack.now` | Force the daily QWK pre-pack event now |
| `data/pack<NNNN>.now` | Pack a QWK message packet for user #`<NNNN>` (4-digit, zero-padded); result lands in `data/file/<NNNN>.qwk` |
| `data/qnet/<hub-id>.now` | Force a QWKnet call-out to that hub |
| `ctrl/sysavail.chat` | Existence indicates sysop is available for chat (users get the page-sysop option) |

### "Won't recycle while in use" gotcha

If a recycle is requested but a server's clients/nodes are still active, the recycle is *deferred* until they all disconnect — it doesn't drop sessions. The `node` utility shows which nodes are flagged `[R]` (recycle pending):

```
node                # show node states (look for [R] = recycle-pending)
node rerun          # toggle the "rerun on next disconnect" flag for ALL nodes
node 3 rerun        # just node 3
```

Servers with the `NO_RECYCLE` option flag in `sbbs.ini` (`[BBS] Options`, `[Web] Options`, etc.) **ignore the recycle semfile**. If touch-recycle isn't working, check that flag first. See `faq:nix#recycle` for the longer Linux-flavoured discussion.

## POSIX signals (sbbscon on *nix)

`sbbscon` writes its PID to a `.pid` file at startup so you can find it (typically `/var/run/sbbs/sbbs.pid` or wherever `PidFile` in `[UNIX]` points). It then runs a dedicated `sigwait()` thread:

| Signal | Handler | Effect |
|--------|---------|--------|
| `SIGTERM`, `SIGINT`, `SIGQUIT`, `SIGABRT` | `_sighandler_quit` | Graceful shutdown — every server stops, the process exits. Equivalent to `touch ctrl/shutdown` for every server at once, with the process going away too. |
| `SIGHUP` | `_sighandler_rerun` | `recycle_all()` — sets `recycle_now=true` on bbs/ftp/web/mail/services. Same effect as `touch ctrl/recycle`, but no file is created on disk. Useful when `ctrl/` is read-only or you want to avoid leaving artefacts. |
| `SIGPIPE`, `SIGALRM` | ignored (`SIG_IGN`) | broken-socket / alarm fall through harmlessly |

`SIGKILL` (and `kill -9`) cannot be caught — they terminate immediately, dropping every open session and skipping cleanup. **Only for hung processes.** Try `SIGTERM` first; if it doesn't exit within a few seconds, then escalate.

## OS service managers

Detect which one this host uses, then prefer the semfile approach for anything granular; use the service manager for full-process restarts and start/stop.

### Linux (systemd)

```
systemctl list-units --type=service | grep -iE 'sbbs|synchro|httpd|ircd'
systemctl status   <unit>
systemctl restart  <unit>     # full process restart — drops all sessions
systemctl reload   <unit>     # sends SIGHUP if the unit is configured for it = recycle-all
systemctl stop     <unit>
systemctl start    <unit>
systemctl kill -s HUP <unit>  # explicit SIGHUP if `reload` isn't wired in
```

`systemctl reload` is the systemd-native way to trigger recycle (via SIGHUP) without restarting the process. Whether `systemctl reload` works depends on whether the unit file declares `ExecReload=/bin/kill -HUP $MAINPID` — if not, `systemctl kill -s HUP <unit>` does the same thing without unit-file changes.

### Linux (sysvinit / OpenRC / others)

```
service sbbs <start|stop|restart|reload>     # SysV-init wrapper
/etc/init.d/sbbs <start|stop|restart|reload> # direct
rc-service sbbs <start|stop|restart>         # OpenRC

# Always available regardless of init system:
kill -HUP  $(cat /var/run/sbbs/sbbs.pid)     # recycle-all via signal
kill -TERM $(cat /var/run/sbbs/sbbs.pid)     # graceful shutdown
```

### macOS (launchd)

```
launchctl list | grep -i sbbs
sudo launchctl kickstart -k system/<label>    # stop + start
sudo launchctl unload  /Library/LaunchDaemons/<plist>
sudo launchctl load    /Library/LaunchDaemons/<plist>
```

The semfile and signal mechanisms work identically on macOS — `kill -HUP $(cat <pidfile>)` recycles without going through launchd.

### Windows (sbbsNTsvcs)

```
sc query   SynchronetBBS                     # status (one per service: BBS/FTP/Mail/Web/Services)
sc stop    SynchronetBBS
sc start   SynchronetBBS
sc pause   SynchronetBBS                     # not all services support this
sbbsNTsvcs install / remove / start / stop   # the bundled wrapper from exec/
services.msc                                 # GUI
```

`sbbsNTsvcs.exe` reads the `SBBSCTRL` environment variable for the ctrl directory path. Semaphore files in that `ctrl/` work the same as on Unix — `echo. > C:\sbbs\ctrl\recycle` does the right thing.

## Windows front-ends

### sbbsctrl (GUI)

The Synchronet Control Panel hosts the servers in-process. Recycle / Pause / Shutdown live in each server's pane menu and in the top-level BBS menu. Closing sbbsctrl shuts down every server it was hosting.

### sbbs.exe (Windows console mode)

Same servers, same code, no GUI; output goes to the console window. Use `Ctrl+C` for graceful shutdown (mapped to `SIGINT`-equivalent) and the semfile mechanism for everything else.

### Choosing between them

| Want | Use |
|------|-----|
| Always-on, survives logout, no user logged in | `sbbsNTsvcs` |
| GUI monitoring + log panes (the `WS<date>.LOG` files come from here — see `synchronet-logs`) | `sbbsctrl` |
| Foreground debugging, console output visible | `sbbs.exe` |

## When MQTT is preferred (even on the same host)

Every operation in this skill has an MQTT-published equivalent: `host/+/recycle`, `host/+/pause`, `host/+/resume`, `host/+/clear`, per-server `server/+/recycle`/`clear`, per-node `node/+/set/*`, etc. See the **`synchronet-mqtt`** skill for broker connect/auth and the full control-plane topic list. MQTT and semfiles are **not mutually exclusive** — both work on the same install, and either is correct for most operations. Reach for MQTT (rather than a semfile or signal) when any of the following applies:

- **You don't have shell access** to the host, but you do have broker credentials (a dashboard, IRC bot, JS module, remote agent).
- **You want positive acknowledgement.** The retained `…/server/+` status topic reflects the new state within a few seconds — `stopping` → `initializing` → `ready` after a recycle confirms it took effect. Semfiles give no feedback channel; you have to watch the journal yourself.
- **You want to fan out across multiple hosts in one shot** (a single publish to `host/+/recycle` reaches every host running this BBSID; semfiles on shared `ctrl/` can do the same but require per-host hostname-suffix files if you want different targets).
- **You're already in MQTT tooling.** A script subscribed to status topics shouldn't bounce out to the filesystem for the action — same channel, immediate, auditable on the broker side.
- **Faster reaction.** Semfile polling happens on `SemaphoreCheckFrequency` (typically a couple of seconds); MQTT delivery is event-driven and effectively immediate.
- **`ctrl/` is on slow / laggy shared storage.** NFS over WAN, intermittently-mounted CIFS, eventually-consistent object storage — mtime detection there can be late or inconsistent. MQTT bypasses the filesystem.
- **Filesystem write access to `ctrl/` is restricted** but the BBS user can publish to its own broker.

Reach for semfiles (rather than MQTT) when:

- **MQTT isn't enabled on the install** (`[MQTT] Enabled=false` in `main.ini`), or you can't reach the broker.
- **You want a durable on-disk record** that the operation was requested (`ctrl/recycle.<host>` sticks around; an MQTT publish disappears once delivered, modulo broker logs).
- **You're scripting from inside the BBS user's shell** with no MQTT client installed and adding `mosquitto-clients` is more friction than `touch`.

**One thing MQTT does *not* buy you**: a bypass for the `NO_RECYCLE` option flag. A server with `NO_RECYCLE` set in `sbbs.ini` ignores *every* recycle path — semfile, SIGHUP, **and** MQTT — because the check is on the `recycle_now` flag itself, which all three mechanisms feed. The only way to apply a config change to a `NO_RECYCLE` server is a full process restart (service-manager `stop`+`start`). Shutdown still works via every mechanism regardless.

## Equivalence reference

| Operation | Semfile (any OS) | Signal (*nix) | systemd | sbbsNTsvcs | MQTT |
|-----------|------------------|---------------|---------|------------|------|
| Recycle all servers | `touch ctrl/recycle` | `kill -HUP <pid>` | `systemctl reload <unit>` or `kill -s HUP` | sbbsctrl menu / restart each service | `host/+/recycle` |
| Recycle one server | `touch ctrl/recycle.<S>` | (no per-server signal) | (restart the per-server unit if split) | `sc stop/start Synchronet<S>` | `host/+/server/<S>/recycle` |
| Shutdown everything | `touch ctrl/shutdown` + wait | `kill -TERM <pid>` | `systemctl stop <unit>` | `sc stop` each | (no single MQTT topic; combine pause+OS stop) |
| Stop accepting (keep existing) | `touch ctrl/pause` | (none) | (none) | `sc pause` (limited) | `host/+/pause` |
| Resume | `rm ctrl/pause` | (none) | (none) | `sc continue` | `host/+/resume` |
| Clear failed-login list | `touch ctrl/clear` | (none) | (none) | (none) | `host/+/clear` (empty payload) |
| Clear one IP | `echo IP > ctrl/clear` | (none) | (none) | (none) | `host/+/clear` (IP as payload) |
| Force a timed event | `touch data/<event>.now` | (none) | (none) | (none) | (none — local only) |

## Safety patterns

- **Verify the lever took effect.** After a recycle, watch the journal (`journalctl -fu <unit>`) or the MQTT `…/server/+` status topic. After a clear, the next deny-at-accept hit from the released IP confirms the list was cleared (or didn't get cleared, if you see fresh `!CLIENT BLOCKED` lines for the same IP straight away).
- **Prefer per-server over global.** `ctrl/recycle.web` only restarts web — five hosts × five servers becomes one host × one server instead of 25 restarts.
- **Use hostname variants on shared `ctrl/`.** Without them, every host mounting the share reacts to the same global semfile. (Diagnose the symptom from the log side via `synchronet-logs`.)
- **`pause` instead of `shutdown` for maintenance.** Pause lets you watch existing sessions drain naturally; shutdown kills them. Resume by `rm`-ing the pause file (or publishing to the MQTT `resume` topic).
- **Touch `sbbs.ini` deliberately or not at all.** It's a recycle trigger via mtime. Backup/sync tools that preserve mtime won't trigger it; tools that don't (`cp` without `-p`, `rsync` without `-t`) will.
- **Beware "stuck" recycles.** If `node` shows nodes flagged `[R]` for a long time, those users are still connected — the recycle is waiting for them. Either let them disconnect or, in an emergency, force-drop via `node <N> down` or the MQTT `node/<N>/set/intr` topic.
- **`kill -9` is a last resort.** It bypasses every cleanup path (sockets stuck in TIME_WAIT, lockfiles left behind, MQTT will-messages not sent). Try SIGTERM and a few seconds of patience first.

## Common mistakes

- **Reaching for `systemctl restart` for a config reload.** That's a full process restart — drops every session on every server. `systemctl reload` (if wired) or `touch ctrl/recycle` does it without the process-restart penalty.
- **Touching `ctrl/recycle` on a shared-storage `ctrl/` directory.** It fires on *every* host that mounts the share. Use `ctrl/recycle.<hostname>` to scope.
- **Expecting `ctrl/pause` to drain by mtime update.** It uses *existence*, not mtime — `touch` to pause, `rm` to resume. (`ctrl/recycle`/`shutdown`/`clear` use mtime; `pause` is the odd one out.)
- **Setting `clear` to clear a specific IP but trailing-newline-truncating the IP.** `echo -n 203.0.113.5 > ctrl/clear` and `echo 203.0.113.5 > ctrl/clear` both work — Synchronet trims trailing whitespace via `truncsp()`. But `printf "203.0.113.5\0junk"` and similar weird payloads might not.
- **Treating `kill -HUP` as a per-server signal.** It isn't — it recycles *every* server in the process (`recycle_all()`). For per-server, use the semfile.
- **`sc stop SynchronetBBS` when meaning "recycle".** That stops the service entirely; you then need a matching `sc start`. There's no native `sc reload` — use the semfile or the MQTT recycle topic.
- **Editing `sbbs.ini` by hand and being surprised by an instant recycle.** Every server watches it. If you're doing a bulk edit, copy it aside, edit the copy, and move it back atomically when ready.
- **Looking for `recycle_now` flags in `main.ini`.** They're runtime in-memory state set by `recycle_all()`/semfile detection, not config-file settings. The `NO_RECYCLE` *option flag* is a config setting (per server), and disables the recycle response.

## Cross-references

- For the file/log side of these operations (the `!Recycling`, `!Shutdown semaphore file detected`, `!Pause semaphore signaled` console lines, and the `data/error.log` entries that follow): the **`synchronet-logs`** skill.
- For the MQTT equivalents (remote control, cross-host fan-out, retained status feedback): the **`synchronet-mqtt`** skill.
- For *building* Synchronet (not running it): the **`synchronet-build`** skill.
- For source-level verification of the semfile naming algorithm: `src/xpdev/semfile.c::semfile_list_init`. For the per-server polling loops: `main.cpp`, `websrvr.cpp`, `mailsrvr.cpp`, `ftpsrvr.cpp`, `services.cpp` — search for `semfile_list_check`. For signal handlers: `src/sbbs3/sbbscon.c` — search for `_sighandler_quit` and `_sighandler_rerun`.
