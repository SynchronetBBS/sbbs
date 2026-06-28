# Wren API TODO

Live TODOs for the SyncTERM Wren scripting API. Historical audit notes belong
in git history, not here.

## Deferred: Wren Main Menu VM

The eventual Wren main-menu implementation should keep the connected and
unconnected contexts as separate VMs/capability sets. The menu VM has no wire
control by default; connected-only terminal/session APIs stay gated away from
it.

Menu code should deal in real `BBS` instances backed by `struct bbslist`, not a
parallel dialing-entry type. Existing connected `BBS.*` static getters can
remain as a compatibility facade over the active/current BBS record while newer
code moves toward the same instance model the menu will use.

Script autoload can use `scripts/auto/menu/*.wren`; the embed/build list still
needs to include that path when the menu VM is added.

## Naming Inconsistencies

These are easiest to fix while scripts are still few.

1. **Enum member casing.** `Codepage.cp1251_b`, `iso8859_4`,
   `petsciiUpper`, `atariSt`, and `ScreenMode.c80x25` mix naming styles.
   Pick one public casing rule and apply it consistently.

2. **Screen helper class names.** `Screen.window` returns `ScreenWindow`,
   `Screen.supports` returns `ScreenSupports`, `Screen.font` returns
   `ScreenFonts`, but `Screen.palette`, `Screen.color`, `Screen.cursor`, and
   `Screen.videoFlags` use shorter or different names. Pick a prefix and
   singular/plural rule.

3. **Directory globals vs Host getters.** `Cache`, `Download`, and `Upload`
   duplicate `Host.cacheDirectory`, `Host.downloadDir`, and `Host.uploadDir`
   with different names. Decide which surface is public.

4. **Trailing underscore public helper.** `Codepage.encodes_(text)` follows
   the project-private naming convention but is used by other modules. Rename
   it to a public API name or make it actually private.

5. **Hyperlinks.add argument name.** Rename the `idParam` argument to
   something that clearly means the OSC 8 `id=` parameter string, not the
   hyperlink ID.

6. **Mouse constants.** `Key` exposes the full function-key family, but
   `Mouse` only names button 1 and wheel constants. Add explicit button 2/3
   constants or document bare-integer use as intentional.

7. **Font constants.** `Font.cp437English` is the only named codepage font
   slot; the rest require numeric lookup. Either name all public slots or
   document numeric slot use as the API.

## Module Organization

`syncterm.wren` still contains several independent concept islands: terminal
I/O, filesystem/cache, SFTP, WON serialization, event scaffolding, and enums.

1. Move `WON` to its own library module.
2. Consider splitting SFTP classes into a dedicated module.
3. Move enum-only classes into a pure Wren module and re-export them.

## Future-Proofing

1. **Integer-backed enums.** `Codepage`, `Font`, and `ScreenMode` expose C
   indices directly. Either commit publicly to never renumber them or switch
   to stable opaque identifiers accepted by foreign methods.

2. **General consent tokens.** `File.token` is currently tied to picker
   consent. Future flows such as edited paths, known-hosts updates, and
   drag-and-drop may need a general `Host.consent(path, reason)` style API.

3. **ScreenMode taxonomy.** The enum mixes dimensions, display families, and
   magic values. Add query helpers such as `ScreenMode.dimensions(mode)` and
   `ScreenMode.familyOf(mode)` so scripts do not parse names.

4. **Hook ownership.** Hooks close over all state and have no owner/user-data
   tag. Add an opt-in owner argument if Apps or modules need bulk teardown.

5. **Hook.dispatch_ override risk.** `Hook.dispatch_` is pure Wren and part of
   dispatcher behavior. Document it as a host contract or harden it against
   accidental module override.

6. **Isolated child fibers.** If a non-App multi-source context appears, add a
   standalone `Fiber.runIsolated(fn)` primitive rather than duplicating
   `App.runChild` patterns outside Apps.

## Footguns To Fix Or Document

1. Add a debug warning when an `Input.pushClaim` handle is GC-popped because
   the script forgot to call `pop()`.

2. Expose `Codepage.cellStorage` so scripts can query the cell-storage
   codepage instead of assuming CP437 forever.

3. Add a short guide explaining when to use `Conn.send` versus `CTerm.write`
   so scripts do not confuse wire output with local terminal output.

4. Remove dead `VideoFlags.expand` setter forward declarations from
   `wren_bind_screen.h`.

## Recommended Order

1. Naming pass.
2. Enum-stability decision.
3. Main-menu VM design.
4. Footgun/docs cleanup.
5. Module split.
