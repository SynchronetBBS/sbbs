# qtmonitor — Synchronet BBS Monitor

Cross-platform monitor and control panel for Synchronet BBS systems,
built with PySide6 (Qt) and MQTT. Connects to Synchronet's built-in
`broker.js` MQTT broker via TLS-PSK.

## Features

- Real-time log viewing for all servers (Telnet, Mail, FTP, Web, Services)
- Node status monitoring with verbose descriptions
- Connected client tracking
- Failed login attempt tracking
- Server state display (ready, paused, stopped, etc.)
- Aggregate statistics (clients, served, errors)
- Server control (recycle, pause, resume, clear login attempts)
- Node control (lock, down, interrupt, rerun, clear errors, send message)
- Force timed events and network callouts
- Dockable/tabbed pane layout with drag-and-drop rearrangement
- Dark/light theme toggle
- Persistent settings and window layout

## Requirements

- Python 3.11+ (3.13+ recommended for native TLS-PSK support)
- PySide6
- paho-mqtt

## Install

```sh
cd src/sbbs3/qtmonitor
pip install .
```

Or run directly:

```sh
pip install -r requirements.txt
python3 qtmonitor.py [options]
```

## Usage

```
qtmonitor -m BROKER_HOST -p 8883 --psk-id ALIAS --psk-key PASSWORD -P SYSPASS
```

| Option | Description |
|--------|-------------|
| `-m HOST` | MQTT broker hostname |
| `-p PORT` | MQTT broker port (8883 for TLS) |
| `-i ID` | BBS system ID for topic prefix (auto-detected if omitted) |
| `-u USER` | MQTT username (not needed for TLS-PSK) |
| `-P PASS` | MQTT password (system password for broker.js) |
| `--psk-id ID` | TLS-PSK identity (sysop alias, lowercased) |
| `--psk-key KEY` | TLS-PSK key (sysop BBS password, lowercased) |

Settings are saved after first use. Subsequent runs need no arguments.

## Broker Setup

qtmonitor connects to Synchronet's `broker.js`, which provides an
MQTT 5.0 broker with TLS-PSK authentication on port 8883.

Currently, `broker.js` must be run via jsexec:

```sh
jsexec broker.js
```

Configure the BBS MQTT connection in scfg under
Networks > MQTT. Set Enabled to Yes, Broker Address and Port to match
broker.js, and TLS Mode to SBBS.

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| Ctrl+, | Settings |
| Ctrl+Q | Quit |
| Ctrl+Shift+R | Reset layout |
| Alt+1..9 | Toggle dock panes |
