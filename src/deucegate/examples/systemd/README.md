# Running DeuceGate as a systemd user service

The supplied `deucegate.service` runs DeuceGate under the current user's
systemd manager. It assumes:

- the executable is `~/.local/bin/deucegate`;
- the GameSrv root is `~/ib-gamesrv`;
- `SSHServerPort` in `config/gamesrv.ini` is an unprivileged port, normally
  1024 or higher.

Edit the unit if either path differs. A per-user service normally cannot bind
port 22; port 2222 is a conventional alternative.

Install and start it with:

```sh
install -Dm755 build/deucegate ~/.local/bin/deucegate
install -Dm644 examples/systemd/deucegate.service \
  ~/.config/systemd/user/deucegate.service
systemd-analyze --user verify ~/.config/systemd/user/deucegate.service
systemctl --user daemon-reload
systemctl --user enable --now deucegate.service
```

The unit runs `--check-config` before each start. Follow its output with:

```sh
journalctl --user --unit deucegate.service --follow
```

The journal is preferred because it handles rotation and does not add another
writable path to the optional sandbox. To use a plain append-only file instead,
add these settings to a drop-in and substitute the desired absolute path:

```ini
[Service]
StandardOutput=append:%h/deucegate.log
StandardError=append:%h/deucegate.log
```

User services normally stop after the user's last login session. To keep
DeuceGate running after logout and start the user manager during boot, enable
lingering for the service account:

```sh
loginctl enable-linger "$USER"
```

Some distributions require an administrator to run that command.

## Optional hardening

`hardening.conf` makes most of the home directory read-only, provides a private
temporary directory, removes capabilities, and enables several namespace and
system-call protections. The restrictions apply to every native door and DOS
emulator launched by DeuceGate, not just the SSH server. Test every configured
door before enabling it. Add every writable location outside `~/ib-gamesrv` to
`ReadWritePaths`, or omit the filesystem restrictions when a door cannot work
within them.

Install the drop-in with:

```sh
install -Dm644 examples/systemd/hardening.conf \
  ~/.config/systemd/user/deucegate.service.d/hardening.conf
systemctl --user daemon-reload
systemctl --user restart deucegate.service
```

The filesystem namespace settings require Linux unprivileged user namespaces.
If the service fails during sandbox setup, inspect the journal and remove the
drop-in rather than weakening unrelated system protections:

```sh
rm ~/.config/systemd/user/deucegate.service.d/hardening.conf
systemctl --user daemon-reload
systemctl --user restart deucegate.service
```

The example deliberately does not use `PrivateDevices`, `ProtectProc`,
`ProcSubset`, `RestrictAddressFamilies`, or a broad `SystemCallFilter`.
`PrivateDevices` and syscall or socket-family restrictions can break DOS
emulators and native doors, while `ProtectProc` and `ProcSubset` are not
supported by the per-user service manager. Add stricter settings only after
testing the complete door configuration.

Resource directives such as `MemoryMax=` and `TasksMax=` may also be added to
the drop-in, but their limits cover DeuceGate and all of its door processes
together. Size them for the heaviest configured door rather than the listener
alone. The example does not raise `LimitNOFILE`: DeuceGate already caps active
connections, and a service-level descriptor limit is inherited by every door.
Raise it only when measured requirements justify doing so.
