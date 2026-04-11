# ZZT Web Sound Bridge Pack

This folder contains a drop-in web bridge for ZZT sound on Synchronet web terminals.

The ZZT door works without this pack.  
Without this bridge, sound calls are ignored (silent/no-op).

## What "fTelnet iframe flow" Means

In this context, it means:

1. `root/js/terminal.js` controls terminal UI in the parent page.
2. `root/terminal-iframe.html` hosts the fTelnet client.
3. The iframe captures FLWEB APC packets (`ESC _ FLWEB/1 ... ESC \`) from terminal output.
4. Packets are sent to parent as `postMessage(type: "terminal-ui")`.
5. Parent calls `FLWeb.handleTerminalUi(...)` in `root/js/flweb.js`.
6. `root/api/flweb-assets.ssjs` resolves shared/xtrn audio assets for browser playback.

## Included Files

- `files/root/terminal.xjs` — standalone multimedia terminal page
- `files/root/terminal-iframe.html`
- `files/root/js/terminal.js`
- `files/root/js/flweb.js`
- `files/root/api/flweb-assets.ssjs`
- `files/root/api/terminal-ui-config.ssjs`
- `files/root/terminal-ui.ini`
- `install-web-bridge.sh`

## Install

This is the intended path for a typical sysop install.

```bash
cd /sbbs/xtrn/zzt/web
./install-web-bridge.sh
```

- The default target is `../../../webv4` (relative to the script), which resolves
  to the `webv4` directory alongside `xtrn/`.
- Pass an explicit path if your webv4 is elsewhere:
  `./install-web-bridge.sh /sbbs/webv4`
- Backups are written to: `<target>/.zzt-web-backup/<timestamp>/...`

### What Gets Deployed

- `<target>/root/terminal.xjs` — **new standalone page** (no manual setup needed)
- `<target>/root/terminal-iframe.html`
- `<target>/root/js/terminal.js`
- `<target>/root/js/flweb.js`
- `<target>/root/api/flweb-assets.ssjs`
- `<target>/root/api/terminal-ui-config.ssjs`
- `<target>/root/terminal-ui.ini`

The installer backs up any existing files before overwriting them.

## Standalone Terminal Page

After install, a self-contained terminal page is available at:

```
http://<your-bbs-host>:<port>/terminal.xjs
```

- No changes to stock webv4 pages, navbar, or layout are required.
- Logged-in users are auto-connected via RLogin.
- Anonymous visitors see a connect button and join via Telnet.
- The FLWEB audio bridge, toast notifications, and all ZZT sound
  features work out of the box on this page.

## Do I Need To Build An Iframe By Hand?

No. The standalone `terminal.xjs` page provides the complete DOM scaffold.
`terminal.js` creates the fTelnet iframe automatically.

Sysops do not need to write any iframe or HTML code.

## Embedding In A Custom Web Shell

If you want to embed the terminal into your own page instead of using
`terminal.xjs`, these are the required DOM elements for `terminal.js`:

```html
<div id="terminal-panel" class="is-hidden">
  <div id="terminal-iframe-container"></div>
</div>
<button id="btn-terminal" type="button">Terminal</button>
<div id="chat-toasts"></div>
```

Set `window.sbbsConfig` before loading the scripts:

```html
<script>
window.sbbsConfig = {
    ftelnet: true,
    hostname: location.hostname,
    wsp: 1123, wssp: 11235,  /* your WS/WSS ports */
    /* ... see terminal.xjs for the full list */
};
</script>
<script src="./js/flweb.js"></script>
<script src="./js/terminal.js"></script>
```

## Manual Install

Copy from this pack:

- `files/root/terminal.xjs` -> `<target>/root/terminal.xjs`
- `files/root/terminal-iframe.html` -> `<target>/root/terminal-iframe.html`
- `files/root/js/terminal.js` -> `<target>/root/js/terminal.js`
- `files/root/js/flweb.js` -> `<target>/root/js/flweb.js`
- `files/root/api/flweb-assets.ssjs` -> `<target>/root/api/flweb-assets.ssjs`
- `files/root/api/terminal-ui-config.ssjs` -> `<target>/root/api/terminal-ui-config.ssjs`
- `files/root/terminal-ui.ini` -> `<target>/root/terminal-ui.ini`

## Config Checklist

- `modopts.ini` (`[web]`) has `ftelnet=true` (this is the default).
- `modopts.ini` (`[web]`) points to your active web root (example: `web_directory=../webv4`).
- WebSocket service (WS/WSS) is enabled in `services.ini`.
- `root/api/flweb-assets.ssjs` is present and executable.
- `root/api/terminal-ui-config.ssjs` and `root/terminal-ui.ini` are present.
- Browser audio requires at least one user interaction (normal autoplay policy).

## Terminal Policy Toggles (`terminal-ui.ini`)

`root/terminal-ui.ini` lets sysops set terminal behavior without editing JS:

- Fixed or auto-fit size:
  - `lock_screen_size=true/false`
  - `columns`, `rows`
  - `min_columns`, `max_columns`, `min_rows`, `max_rows`
- Optional features:
  - `enable_touch_handlers`
  - `enable_mouse_wheel_arrows`
  - `enable_render_speed_patch`
  - `enable_parent_audio_unlock_bridge`
  - `enable_canvas_scale`

## Verification

1. Navigate to `http://<host>:<port>/terminal.xjs`.
2. Click the power button (or wait for auto-connect if logged in).
3. Run `EXPERIMENT3` from `experiment3.js` (if installed as xtrn).
4. Press `B` in Experiment3 and confirm bridge detect success.
5. Run `ZZT`.
6. Trigger sound effects (fire, pickups, doors, bombs).
7. Load a world pack with MP3s and confirm background music playback.

## Rollback

Restore files from `<target>/.zzt-web-backup/<timestamp>/...` if you want to revert to the previous terminal stack.

## Notes

- Stock webv4 does not include the FLWEB bridge by default — this pack adds it.
- The standalone `terminal.xjs` page does not modify any existing webv4 pages.
- Non-web clients (telnet/ssh) continue to function; they just remain silent.
