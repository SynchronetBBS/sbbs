// status_default.wren — default Wren-driven status bar.
//
// Matches the historical C update_status() layout:
//
//   " name (flags) (speed) │ Conn   │ Connected: HH:MM:SS │ ALT-Z menu "
//                          ^^                                            ^
//                          fixed col 27..30 indicator overlay            menu's
//                          (log ‼, SFTP ↑, SFTP ↓, mouse M)              last
//                                                                        char
//                                                                        at width-2
//
// Right-anchored: the name field expands to push the menu hint to one
// cell from the right edge.  The 4-cell indicator block stays at fixed
// columns 27..30 (mirroring the C original) so individual indicators
// lighting up or going dark never shift any other content.
//
// To override: drop a same-named file in your auto-load dir, or set
// `Status.callable = ...` from any later script.

import "syncterm" for Status, Surface, BBS, Capture, CTerm, Host, ConnType

class StatusDefault {
  // Cell colour palette (RGB; matches the C original's vmem_cell.fg
  // values + the corresponding 4-bit legacy_attr fallback).
  static FG_NORMAL { 0xffff54 }       // bright yellow
  static FG_DIM    { 0x545454 }       // dim grey (mouse-disabled)
  static FG_RED    { 0xff5454 }       // bright red (log error)
  static SEP_BYTE  { 0xb3 }           // CP437 │
  static UP_BYTE   { 0x18 }           // CP437 ↑
  static DOWN_BYTE { 0x19 }           // CP437 ↓
  static LOG_BYTE  { 0x13 }           // CP437 ‼
  static M_BYTE    { 0x4d }           // 'M'

  // Indicator block is 4 cells wide.  Pinned to the right end of the
  // name area (just before the " │ ConnType " separator) so it never
  // wastes space when the name is shorter than the available room.

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

  // Place a single CP437 byte at column `col`, leaving the cell's
  // pre-filled colour intact.  No-op if out of bounds.
  static putByte_(surf, col, byte) {
    if (col < 0 || col >= surf.width) return
    surf[col].chByte = byte
  }

  // Write " │ " (space, separator, space) starting at `col`.  Returns
  // the number of cells written (3 if it fits, fewer if clipped).
  static writeSep_(surf, col) {
    var w = surf.width
    var written = 0
    if (col < w) {
      putByte_(surf, col, 0x20)
      written = 1
    }
    if (col + 1 < w) {
      putByte_(surf, col + 1, StatusDefault.SEP_BYTE)
      written = 2
    }
    if (col + 2 < w) {
      putByte_(surf, col + 2, 0x20)
      written = 3
    }
    return written
  }

  // Right-pad a String with spaces to exactly `n` bytes (assumes
  // ASCII, like the connection-type names).  Truncates if longer.
  static padTo_(s, n) {
    var len = s.bytes.count
    if (len == n) return s
    if (len > n) {
      var bs = s.bytes
      var t = ""
      for (i in 0...n) t = t + String.fromCodePoint(bs[i])
      return t
    }
    var t = s
    var pad = n - len
    for (i in 0...pad) t = t + " "
    return t
  }

  // Compose the BBS name plus mode flags plus inline (speed),
  // matching the C original's nbuf assembly order: SAFE, Logging,
  // (speed), DrWy, OOTerm*, INV.  Speed comes from the live
  // throttleSpeed for network connections (Alt-Up/Down adjustable),
  // or the configured port rate for serial.
  static nameWithFlags_() {
    var n = BBS.name
    if (Host.safeMode) n = n + " (SAFE)"
    if (Capture.active) n = n + " (Logging)"
    var ct = BBS.connType
    var isSerial = ct == ConnType.serial || ct == ConnType.serialNoRts
    var rate = isSerial ? BBS.bpsRate : CTerm.throttleSpeed
    if (rate > 0) n = n + " (%(rate))"
    if (CTerm.doorwayMode) n = n + " (DrWy)"
    var ooii = CTerm.ooiiMode
    if (ooii == 1) n = n + " (OOTerm)"
    if (ooii == 2) n = n + " (OOTerm1)"
    if (ooii == 3) n = n + " (OOTerm2)"
    if (CTerm.atasciiInverse) n = n + " (INV)"
    return n
  }

  // "HH:MM:SS", or "Too Long" past the 100-hour C clamp.
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

  // Truncate ASCII string `s` to fit in `room` bytes, replacing the
  // tail with "..." when truncation happens.  Returns "" when
  // room < 1.
  static truncate_(s, room) {
    if (room <= 0) return ""
    var bs = s.bytes
    var n = bs.count
    if (n <= room) return s
    if (room < 4) {
      var t = ""
      for (i in 0...room) t = t + String.fromCodePoint(bs[i])
      return t
    }
    var keep = room - 3
    var t = ""
    for (i in 0...keep) t = t + String.fromCodePoint(bs[i])
    return t + "..."
  }

  // Indicators painted near the right end of the name area, in the
  // historical order: log ‼, SFTP ↑, SFTP ↓, mouse M.  `nameEnd`
  // is the rightmost column of the name area (1-indexed); the M
  // sits one cell in from that so a padding space buffers it from
  // the " │ " separator.  Each cell either gets its glyph + colour
  // or stays as the prefilled space — the indicator block reserves
  // no layout space, so other slots never shift when an indicator
  // lights up or goes dark.
  static paintIndicators_(surf, nameEnd) {
    if (nameEnd < 5) return
    paintLog_(surf,    nameEnd - 4)
    paintArrow_(surf,  nameEnd - 3, StatusDefault.UP_BYTE,   Host.uploadArrow)
    paintArrow_(surf,  nameEnd - 2, StatusDefault.DOWN_BYTE, Host.downloadArrow)
    paintMouse_(surf,  nameEnd - 1)
  }

  static paintLog_(surf, col) {
    if (col < 0 || col >= surf.width) return
    if (Host.logUnreadError) {
      var c = surf[col]
      c.chByte = StatusDefault.LOG_BYTE
      c.fgRgb = StatusDefault.FG_RED
      c.legacyAttr = 0x1c
    } else if (Host.logUnread) {
      surf[col].chByte = StatusDefault.LOG_BYTE
    }
  }

  static paintArrow_(surf, col, byte, lit) {
    if (lit) putByte_(surf, col, byte)
  }

  static paintMouse_(surf, col) {
    if (col < 0 || col >= surf.width) return
    if (CTerm.mouseMode == 0) return
    var c = surf[col]
    c.chByte = StatusDefault.M_BYTE
    if (CTerm.mouseDisabled) {
      c.fgRgb = StatusDefault.FG_DIM
      c.legacyAttr = 0x18
    }
  }

  // Compute total right-side slot width (each non-empty contributes
  // 3 chars for " │ " plus its content).
  static rightWidth_(slots) {
    var total = 0
    for (s in slots) {
      if (s != "") total = total + 3 + s.bytes.count
    }
    return total
  }

  // Render entry point: called by the host with a width×1 Surface
  // pre-filled to yellow-on-blue spaces.  Mutate in place.
  static render(surf) {
    if (!Status.enabled) return
    var w = surf.width
    if (w < 8) return

    // Right-anchored slots after the name field.  At narrow widths
    // (< 80) drop elapsed-time and menu-hint slots.  Conn type is
    // padded to 6 chars so its column doesn't drift when "SSH" gives
    // way to "Telnet".
    var name = nameWithFlags_()
    var conn = padTo_(BBS.connTypeName, 6)
    var time = w >= 80 ? "Connected: %(elapsedString_())" : ""
    var menu = w >= 80 ? "ALT-Z menu" : ""

    // Total width = 1 (leading space) + nameWidth + rightWidth + 1
    // (trailing space).  Solve for nameWidth.  If the name would
    // have to truncate to fit, drop "Connected: " first (saves 11
    // chars) before resorting to "..." -- the label is decoration,
    // the BBS name is information.
    var slots = [conn, time, menu]
    var rightWidth = rightWidth_(slots)
    var nameWidth = w - rightWidth - 2
    if (name.bytes.count > nameWidth && time != "") {
      time = elapsedString_()
      slots = [conn, time, menu]
      rightWidth = rightWidth_(slots)
      nameWidth = w - rightWidth - 2
    }
    if (nameWidth < 1) nameWidth = 1

    name = truncate_(name, nameWidth)

    // Paint name and pad to nameWidth with spaces.
    var col = 1
    col = col + writeStr_(surf, col, name)
    while (col < 1 + nameWidth) {
      putByte_(surf, col, 0x20)
      col = col + 1
    }
    // Right-side slots in order.
    for (s in slots) {
      if (s == "") continue
      col = col + writeSep_(surf, col)
      col = col + writeStr_(surf, col, s)
    }

    // Indicator overlay last so the glyphs land on top of the name
    // area's right edge (padding when name is shorter than the
    // available room; the name's tail characters when not).
    paintIndicators_(surf, nameWidth)
  }
}

Status.callable = Fn.new { |surf| StatusDefault.render(surf) }
