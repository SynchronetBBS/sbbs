# qtmonitor — Synchronet BBS Monitor

PySide6/MQTT-based cross-platform replacement for the Windows-only `ctrl/` panel.

## Architecture

- `qtmonitor.py` — entry point, argument parsing
- `mainwindow.py` — QMainWindow with dockable panes, menus, toolbar, status bar
- `mqtt_client.py` — paho-mqtt wrapper bridged to Qt signals via QueuedConnection
- `tls_psk.py` — TLS-PSK support (native on Python 3.13+, ctypes fallback for older)
- `log_widget.py` — colour-coded log pane with level filtering and pause
- `node_widget.py` — terminal node status list
- `client_widget.py` — connected clients list
- `stats_widget.py` — system statistics display
- `login_attempts_widget.py` — failed login attempts tracker
- `settings_dialog.py` — MQTT connection settings

## MQTT

Connects to Synchronet's `broker.js` (MQTT 5.0, TLS-PSK on port 8883).
Topic prefix: `sbbs/{sys_id}/` where sys_id is the BBS QWK ID (e.g. SYNCNIX).

broker.js auth for TLS-PSK connections:
1. TLS-PSK handshake: identity = sysop alias (lowercased), key = sysop password (lowercased)
2. MQTT CONNECT password = system password (`system.check_syspass()`)

## Build / Install

```
pip install -r requirements.txt    # or: pip install .
qtmonitor -m HOST -p 8883 --psk-id ALIAS --psk-key PASSWORD -P SYSPASS
```

Settings persist via QSettings after first run — CLI args override and save.

## TLS-PSK notes

The ctypes fallback (`tls_psk.py`) is fragile:
- Extracts `SSL_CTX*` from CPython's `PySSLContext` at a hardcoded struct offset
- Uses `ldd` to find the correct libssl (Unix-only)
- Uses `@SECLEVEL=0` in cipher string (cryptlib serves 1536-bit DH)
- Prefer Python 3.13+ which has native `ssl.SSLContext.set_psk_client_callback()`

## Conventions

- All paho callbacks run on paho's background thread; signals use `Qt.QueuedConnection`
  to marshal to the GUI thread
- Dark/light theme toggled via View > Dark Mode; widget `set_dark(bool)` methods
- Dock references stored in `_log_docks` dict, not traversed via `parent().parent()`
