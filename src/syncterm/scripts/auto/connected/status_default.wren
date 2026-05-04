// status_default.wren — default Wren-driven status bar.
//
// Installs a render callable that paints the status row in the same
// shape as the original C update_status(), with two intentional
// improvements:
//
//   * Speed lives in its own slot to the right of the connection
//     type, so long serial speeds (921600, 1500000) no longer eat
//     into the BBS-name field.
//   * Indicators (mouse mode, SFTP arrows) right-pin to the row's
//     right edge instead of fixed columns 27..30, so they stay
//     readable at any width.
//
// To override: drop a same-named file in your auto-load dir, or set
// `Status.callable = ...` from any later script.

import "syncterm" for Status, Surface, BBS, CTerm, Host

class StatusDefault {
  // Cell colour palette (RGB; matches the C original).
  static FG_DIM    { 0x545454 }       // dim grey (mouse-disabled)
  static SEP_BYTE  { 0xb3 }           // CP437 │
  static UP_BYTE   { 0x18 }           // CP437 ↑
  static DOWN_BYTE { 0x19 }           // CP437 ↓

  // Write CP437 bytes from `s` into row 0 of `surf` starting at
  // column `x`.  Stops at the right edge or the end of `s`.  Returns
  // the number of cells actually written.
  static writeStr_(surf, x, s) {
    var w = surf.width
    var bytes = s.bytes
    var n = bytes.count
    var written = 0
    for (i in 0...n) {
      var col = x + i
      if (col < 0 || col >= w) break
      surf[col].chByte = bytes[i]
      written = written + 1
    }
    return written
  }

  // Compose the BBS name plus mode flags.  Same flag set as the C
  // original; SAFE comes first so the user can't miss it.
  static nameWithFlags_() {
    var n = BBS.name
    if (Host.safeMode) n = n + " (SAFE)"
    if (CTerm.logMode != 0) n = n + " (Logging)"
    if (CTerm.doorwayMode) n = n + " (DrWy)"
    var ooii = CTerm.ooiiMode
    if (ooii == 1) n = n + " (OOTerm)"
    if (ooii == 2) n = n + " (OOTerm1)"
    if (ooii == 3) n = n + " (OOTerm2)"
    if (CTerm.atasciiInverse) n = n + " (INV)"
    return n
  }

  // "HH:MM:SS", or "Too Long" when the session has been up for over
  // 100 hours (matches the C original).
  static elapsedString_() {
    var s = BBS.elapsedSeconds
    if (s >= 360000) return "Too Long"
    if (s < 0) s = 0
    var hh = (s / 3600).floor
    var mm = ((s / 60) % 60).floor
    var ss = (s % 60).floor
    return "%(pad2_(hh)):%(pad2_(mm)):%(pad2_(ss))"
  }
  static pad2_(n) { n < 10 ? "0%(n)" : "%(n)" }

  // Place a single CP437 byte at column `col`, leaving the cell's
  // pre-filled colour intact.
  static putByte_(surf, col, byte) {
    if (col < 0 || col >= surf.width) return
    surf[col].chByte = byte
  }

  // Right-pinned indicators: SFTP ↑, SFTP ↓, mouse M.  Each takes one
  // cell at the right edge.  Skipped silently when the row is too
  // narrow.  All cells use the default yellow-on-blue except disabled
  // mouse (dim grey).
  static paintIndicators_(surf) {
    var w = surf.width
    if (w < 4) return
    paintMouse_(surf, w - 1)
    if (Host.downloadArrow) putByte_(surf, w - 2, DOWN_BYTE)
    if (Host.uploadArrow)   putByte_(surf, w - 3, UP_BYTE)
  }

  static paintMouse_(surf, col) {
    if (CTerm.mouseMode == 0) return
    var c = surf[col]
    c.chByte = 0x4d                 // 'M'
    if (CTerm.mouseDisabled) c.fgRgb = FG_DIM
  }

  // Write " │ " (space, separator, space) starting at column `col`.
  // Returns the number of cells written (0..3, may be clipped at the
  // right edge).
  static writeSep_(surf, col) {
    var w = surf.width
    var written = 0
    if (col < w)     { putByte_(surf, col, 0x20);     written = 1 }
    if (col + 1 < w) { putByte_(surf, col + 1, SEP_BYTE); written = 2 }
    if (col + 2 < w) { putByte_(surf, col + 2, 0x20); written = 3 }
    return written
  }

  // Render entry point: called by the host with a width×1 Surface
  // pre-filled to yellow-on-blue spaces.  Mutate in place.
  static render(surf) {
    if (!Status.enabled) return
    var w = surf.width
    if (w < 8) return                // too narrow for anything sensible

    // Reserve 3 cells on the right for indicators when there's room.
    var indW = w >= 16 ? 3 : 0
    var bodyW = w - indW
    var col = 1                      // leading space, matching the C original

    // Layout slots, in order, each preceded by " │ " (except the first):
    //   name+flags, connType, speed, elapsed, menu hint.
    // Slots that are empty strings are skipped.  Slot text is
    // truncated to fit the remaining body width if needed.
    var name    = nameWithFlags_()
    var conn    = BBS.connTypeName
    var speed   = BBS.bpsRate > 0 ? "%(BBS.bpsRate) bps" : ""
    var elapsed = w >= 80 ? elapsedString_() : ""
    var menu    = w >= 80 ? "ALT-Z menu" : ""

    var slots = [name, conn, speed, elapsed, menu]
    var first = true
    for (s in slots) {
      if (s == "") continue
      if (!first) {
        if (col + 3 > bodyW) break
        col = col + writeSep_(surf, col)
      }
      first = false
      var room = bodyW - col
      if (room <= 0) break
      var text = s
      if (text.bytes.count > room) {
        // Crude byte truncation; safe because every label this writes
        // is ASCII (no multi-byte UTF-8).
        var trimmed = ""
        var bs = text.bytes
        for (i in 0...room) trimmed = trimmed + String.fromCodePoint(bs[i])
        text = trimmed
      }
      col = col + writeStr_(surf, col, text)
    }

    paintIndicators_(surf)
  }
}

Status.callable = Fn.new { |surf| StatusDefault.render(surf) }
