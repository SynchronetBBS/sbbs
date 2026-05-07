# qtmonitor — Synchronet BBS Monitor

C++ Qt6 cross-platform replacement for the Windows-only `ctrl/` panel.

## Architecture

- `main.cpp` — entry point, argument parsing
- `mainwindow.h/.cpp` — QMainWindow with dockable panes, menus, toolbar, status bar
- `mqttclient.h/.cpp` — QMqttClient wrapper with QSslSocket TLS-PSK, topic dispatch
- `logwidget.h/.cpp` — colour-coded log pane with level filtering and pause
- `nodewidget.h/.cpp` — terminal node status list
- `clientwidget.h/.cpp` — connected clients list
- `statswidget.h/.cpp` — system statistics display
- `loginattemptswidget.h/.cpp` — failed login attempts tracker
- `settingsdialog.h/.cpp` — MQTT connection settings

## MQTT

Connects to Synchronet's `broker.js` (MQTT 5.0, TLS-PSK on port 8883).
Topic prefix: `sbbs/{sys_id}/` where sys_id is the BBS QWK ID (e.g. SYNCNIX).

broker.js auth for TLS-PSK connections:
1. TLS-PSK handshake: identity = sysop alias (lowercased), key = sysop password (lowercased)
2. MQTT CONNECT password = system password (`system.check_syspass()`)

## TLS-PSK

Uses Qt's native QSslSocket with `preSharedKeyAuthenticationRequired` signal.
PSK ciphers are filtered from `QSslConfiguration::supportedCiphers()` —
`setCiphers(QString)` doesn't accept OpenSSL directives like `@SECLEVEL`.
The socket manages TLS independently; QMqttClient receives it as an IODevice
transport after the handshake completes.

## Build

```sh
cmake -S . -B build && cmake --build build
```

Requires: Qt6 (Core, Gui, Widgets, Network, Mqtt), CMake 3.22+, C++17.

## Conventions

- QMqttClient is created fresh in connectToBroker() to avoid nullptr warnings
- SSL errors prompt the user; accepted errors are remembered for reconnects
- Dark/light theme toggled via View > Dark Mode; widget `setDark(bool)` methods
- Dock references stored in `m_logDocks` hash, not traversed via parent()
