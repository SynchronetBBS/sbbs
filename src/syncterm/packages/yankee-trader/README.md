# Yankee Trader control panel

`yankee_trader.wren` converts the scripts, calculators, log-cleanup steps,
recordkeeping system, and command helpers documented at `yt.starflt.com` into a
connected SyncTERM Wren application.

## Install and open

This is an optional user package; none of its Wren modules are compiled into
SyncTERM. Install the four files from this package's `scripts/` directory into
your personal SyncTERM scripts directory:

| Platform | Destination |
| --- | --- |
| Linux / BSD | `$XDG_DATA_HOME/syncterm/scripts/` or `~/.local/share/syncterm/scripts/` |
| macOS | `~/Library/Application Support/SyncTERM/scripts/` |
| Windows | `%APPDATA%\SyncTERM\scripts\` |
| Haiku | `~/config/settings/SyncTERM/scripts/` |

On Unix-like systems, `./install.sh` performs the copy. On Windows, run
`install.ps1` in PowerShell. Both installers accept an optional destination
directory and preserve replaced files with a `.bak` suffix.

In SyncTERM, edit the Yankee Trader BBS entry, open **Wren Scripts**, and add
the module `yankee_trader` (without a path or `.wren` extension). Its three
library modules load automatically when the connection script starts.

Connect to the Yankee Trader BBS and press **Alt-Y**. State is stored in
SyncTERM's cache directory for that BBS. `yankee-trader-games.won` is a small
game registry; every game then has independent `gN-profile.won`,
`gN-universe.won`, and `gN-records.won` files under the `yankee-trader-`
prefix. Changing settings therefore never serializes a C;10 path map, and
changing one game never rewrites another game's data. The stable `gN` identity
survives game renames.

The cache and whole-state clipboard export use compact WON so large saved C;10
path maps do not acquire megabytes of pretty-printing whitespace.

The first two Alt-Y choices provide quick access to replacement `C;3` travel
and adjacent-sector fighter placement. Other live tools remain grouped in the
**Live tools** submenu. Start command-sending actions at the Main or Computer
command prompt shown in the tool's help. Live `C;10` and sensor results are
parsed and saved directly. Sector actions now show their target count, batches,
and turn/resource risk before offering **Run now**, **Preview**, optional
**Copy**, or **Cancel**. Direct runs send one batch at a time and wait for a
returned Main-command Help screen before advancing. Completion and failure
status popups appear immediately; reopening the Alt-Y panel is not required.

## Multiple games on one BBS

One BBS cache can contain any number of independent Yankee Trader games. Use
**Switch game** in the Alt-Y menu, or **Profile and data**, to add,
rename, switch, or delete games. The active game's name remains visible in every
major submenu.

Each game has its own maps, planets, notes, port pairs, tracked locations,
analyses, benchmarks, and settings. Configurable settings include:

- version and universe sector count;
- turns per day, maximum planets and ports, sector-avoid slots, and macro
  repeat count;
- missiles, plasma bolts, Xannor, mercenaries, sensor robots, Danger Scanner,
  and spies.

Feature-dependent calculators and in-game actions are hidden when that
feature is disabled. Switching games cancels pending live automation so a scan
cannot accidentally update another game's records.

From the game's **Main Command** prompt, choose **Profile and data → Detect
game settings** to populate a profile from the connected door. Detection reads
the Info, Version, main-command Help, and Computer screens, then determines the
sector ceiling with a zero-turn binary search using C;2 port-report validity.
The supported 1–99,999 sector range takes no more than 17 probes. Before saving,
the panel shows the proposed values and separates screen-detected values from
limits and features inferred from the detected version.

Some screens report player state rather than game configuration. In particular,
Info shows turns currently remaining, not necessarily the configured daily
allocation. The review therefore asks whether the player has taken, spent, or
gained any turns since the daily reset. It uses the displayed value only when
the user confirms it is untouched; otherwise it asks for the configured turns
per day. Planet and port limits, avoid slots, and features with no safe player
query can use documented version defaults or retain the profile's current
values, at the user's choice. Nothing is applied until the final confirmation.

## Included features

- The replacement `C;3` route scanner, adjacent-sector fighter breadcrumbs,
  arbitrary-viewpoint `C;10` sweeps with response-time progress and ETA, and
  the T1/T2/T3 server benchmarks. Live path results are captured into the
  active game's universe WON file, checkpointed every 100 queries, and flushed
  when a scan completes, is cancelled, changes games, or disconnects. T3 sequences its 20
  searches on returned Computer prompts and stops its clock on the twentieth
  result; leaving the Computer menu is not included in the benchmark time.
- The website plasma-distance calculator and all formulas recovered from
  `ytcalc.xls`: plasma and missile damage, staged kill estimates, safe planet
  shots, daily planet production, planet-bank estimates, productivity
  investment, Xannor planning, mercenary bribes, fighter surrender, and port
  earnings. Each calculator presents all inputs together and recalculates
  immediately as values change. Results are word-wrapped below the fields;
  the taller plasma damage/safe-shot form keeps its results alongside.
- Native replacements for `ytports.py`, `10-scan.py`, the 1,000/3,000-sector
  ZOC sweeps, missile scripts, planet scans, sensor-robot scripts, avoid lists,
  and Mercenary surround scripts.
- Automatic analysis of live `C;10` output and sensor scans, plus isolated
  legacy/recovery import for `C;10` CSV and session logs. Analysis retains the actual
  target-to-path map and identifies direct outgoing warps from the selected
  viewpoint, unique coverage candidates, dead-end probes, long-number-gap
  probes, and possible 20–120-sector rift entrances. The saved-map browser lists the
  sectors and directed edges in every category, finds the captured target
  routes containing a selected feature, and supports direct target-path
  lookup. Generated actions run in reviewed, monitored batches; copying is an
  optional fallback.
- Per-game sector, planet, port-pair, revealed-location, Xannor, mercenary,
  fighter-group, black-hole, and intelligence notes. All games for the BBS can
  be exported/imported together as safe Wren Object Notation (WON).
- Active F1–F12 Yankee Trader key bindings, with an in-app binding reference.
  ZOC's `/R20` and `/R30` suffixes are expanded into native command strings
  using the selected game profile.
- Stock 3.6 3,000-sector, extracted 3.6G-mod 2,004-sector, and Yankee Trader
  3.2 1,000-sector starting profiles, plus per-game overrides for custom BBS
  configurations. The live settings detector records the detected build and
  per-field provenance with the game profile.
- Condensed in-app reference material covering daily routines, money, planets,
  warfare, Xannor, mercenaries, mapping, and advanced techniques.

## Version notes

The starting profiles were checked against `CHANGES.TXT`, `YTINSTR.DOC`, and
`YTSYSOP.DOC` from the extracted 3.6G distribution. Stock 3.2 has 1,000
sectors, 75 planets, 300 ports, 30 repeat commands, and sensor robots; it
already includes missiles, Xannor, and mercenaries, but predates plasma bolts.
Stock 3.6 raises the documented limits to 3,000 sectors, 100 planets, and 1,000
ports, lowers repeats to 20, removes sensor robots, and adds Danger Scanners
and spies. Its change log also says pathfinding was moved from memory to disk
to avoid memory shortages, explicitly noting that this slows autopilot down.
The `C;10` mapper therefore reports each door response time and its running
average; it cannot remove that server-side disk cost.

The extracted 2024-modified 3.6G executable is not identical to that stock
description: a controlled `YT-INIT.EXE` run generated sectors 1 through 2004,
and the executable banner identifies missiles as disabled. Its supplied preset
captures those values. Every limit and feature remains editable because BBS
operators commonly run patched executables or retain older data files.

## Legacy and recovery imports

Normal live tools do not require the clipboard. The following inputs are
isolated under **Universe and log tools → Legacy and recovery imports**.
For **C;10 CSV import**, copy the comma-separated path rows produced by the old
scan. The first row normally represents target sector 8; the panel asks for
this value so partial captures also work.

For **session-log analysis**, copy plain text containing blocks such as:

```text
Sector: 34
Port: A
Warps lead to: 37, 35
```

Imported and live sensor blocks update the saved sector map. **Scan saved
ports** builds its reviewed live action from ports in that map rather than
reading the clipboard again.

## Verification

From the SyncTERM source directory:

```sh
./run_wren.sh packages/yankee-trader/tests/yankee_trader_test.wren
./run_wren.sh packages/yankee-trader/scripts/yankee_trader.wren
```

The first command exercises multi-game state, formula and calculator-form
examples, parsers, analysis, and command generation. The second compiles and
loads the connected application and all of its modules under the headless
SyncTERM harness.

The live prompt matchers target the English prompts used by the archived ZOC
scripts. A game fork that changes those strings will need corresponding matcher
updates. Destructive or turn-consuming actions remain explicit user choices;
the application does not auto-launch a sweep when loaded.
