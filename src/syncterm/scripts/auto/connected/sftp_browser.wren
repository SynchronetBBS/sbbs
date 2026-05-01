// SFTP directory browser — modal Wren replacement for the C-side
// sftp_browser.c.  Triggered by Alt-S (CIO_KEY_ALT_S = 0x1F00) during
// a connected session that negotiated SFTP.  Read-only directory
// listing with navigation; transfers and queue interaction land later
// (see sftp_integration_state memory, steps 8/9).
//
// The hook spawns a child fiber whose entry runs an `App` instance.
// App.run() yields on Input.nextEvent + the SFTP result queue, so
// doterm() keeps draining results during the browser's lifetime.
// CTerm.suspended is flipped while the browser owns the screen so
// connection bytes don't paint over the UI.

import "syncterm"     for Hook, SFTP, SFTPError, CTerm, Screen, Key
import "ui_app"       for App
import "ui_pane"      for Pane
import "ui_list"      for ListView
import "ui_widget"    for Rect
import "ui_popup"     for Alert
import "ui_statusbar" for StatusBar

// One row in the browser.  Holds the raw filename (used for path
// joins on Enter), the pre-formatted display line, and the
// short-description text the bottom bar shows.
//
// The lname@syncterm.net extension carries different semantics per
// entry type on the Synchronet server (sbbs3/sftp.cpp): for libs/dirs
// it's the friendly long name, for files it's the one-line file
// description.  So:
//   * directories — lname (when present) replaces the filename in
//                   the display column; bottom bar is blank.
//   * files       — filename stays in the display column; lname goes
//                   to the bottom bar as the short description.
// The extended (multi-line) description is a separate
// descs@syncterm.net request, not used here yet.
class Row {
  construct new(name, isDir, isParent, size, mtime, shortDesc, hasLongDesc, line) {
    _name        = name
    _isDir       = isDir
    _isParent    = isParent
    _size        = size
    _mtime       = mtime
    _shortDesc   = shortDesc
    _hasLongDesc = hasLongDesc
    _line        = line
  }

  name        { _name        }
  isDir       { _isDir       }
  isParent    { _isParent    }
  size        { _size        }
  mtime       { _mtime       }
  shortDesc   { _shortDesc   }
  hasLongDesc { _hasLongDesc }
  line        { _line        }
}

// ListView subclass that fires `onSelectionChange` whenever the
// cursor lands on a different row.  Used to drive the description
// status bar's async fetch.
class SftpListView is ListView {
  construct new() {
    super()
    _onSelectionChange = null
  }

  onSelectionChange=(fn) { _onSelectionChange = fn }

  selected=(i) {
    super.selected = i
    if (_onSelectionChange != null) _onSelectionChange.call(selectedItem)
  }

  // Row.line is pre-formatted in loadDir; ListView truncates / pads.
  formatItem(item, width) { item.line }
}

class SftpBrowser {
  // ----- Path helpers --------------------------------------------------

  static joinPath(base, name) {
    if (base.count == 0)               return "/" + name
    if (base[-1] == "/")               return base + name
    return base + "/" + name
  }

  // Strip the last path component.  All math is byte-indexed so
  // trailing-slash and leading-slash trimming is unambiguous on
  // UTF-8 paths.
  static parentPath(path) {
    if (path == null) return "/"
    var n = path.bytes.count
    if (n == 0) return "/"
    while (n > 1 && path[n - 1] == "/") n = n - 1
    while (n > 0 && path[n - 1] != "/") n = n - 1
    if (n == 0) return "/"
    if (n > 1)  n = n - 1
    if (n == 0) return "/"
    return path[0...n]
  }

  // ----- Display formatting --------------------------------------------

  // 10-char right-aligned size column.  Directories render as <DIR>;
  // files use a binary-suffix abbreviation.  Integer math only —
  // Wren has no built-in printf.
  static formatSize(size, isDir) {
    if (isDir) return "     <DIR>"
    var v = size
    var unit = "B  "
    if (v >= 1024) {
      v = (v / 1024).floor
      unit = "KiB"
    }
    if (v >= 1024) {
      v = (v / 1024).floor
      unit = "MiB"
    }
    if (v >= 1024) {
      v = (v / 1024).floor
      unit = "GiB"
    }
    if (v >= 1024) {
      v = (v / 1024).floor
      unit = "TiB"
    }
    var s = "%(v)%(unit)"
    while (s.count < 10) s = " " + s
    return s
  }

  // YYYY-MM-DD UTC, or 10 spaces when mtime is zero / missing.  Civil
  // calendar arithmetic from Howard Hinnant's days-from-civil inverse;
  // Wren has no time API.
  static formatDate(mtime) {
    if (mtime == 0) return "          "
    var days   = (mtime / 86400).floor + 719468
    var bias   = days >= 0 ? days : days - 146096
    var era    = (bias / 146097).floor
    var doe    = days - era * 146097
    var yoeNum = doe - (doe / 1460).floor + (doe / 36524).floor
    yoeNum     = yoeNum - (doe / 146096).floor
    var yoe    = (yoeNum / 365).floor
    var y      = yoe + era * 400
    var doy    = doe - 365 * yoe - (yoe / 4).floor + (yoe / 100).floor
    var mp     = ((5 * doy + 2) / 153).floor
    var d      = doy - ((153 * mp + 2) / 5).floor + 1
    var m      = mp < 10 ? mp + 3 : mp - 9
    if (m <= 2) y = y + 1
    var ms = m < 10 ? "0%(m)" : "%(m)"
    var ds = d < 10 ? "0%(d)" : "%(d)"
    return "%(y)-%(ms)-%(ds)"
  }

  // Display label rules — see Row's class comment for the lname
  // semantics.  Directories prefer the lname (friendly name); files
  // always show their bare filename, since their lname is the short
  // description that goes in the bottom bar instead.  hasLongDesc
  // (server-asserted descs@syncterm.net presence in the fattr) shows
  // a `+` marker after the date column so users know F2 has something
  // to fetch.
  static formatRow(name, isDir, size, mtime, lname, hasLongDesc) {
    var label = name
    if (isDir && lname != null && lname.count > 0) label = lname
    var mark = hasLongDesc ? "+" : " "
    return "%(formatSize(size, isDir)) %(formatDate(mtime)) %(mark) %(label)"
  }

  // Byte-wise less-than for two Strings.  Wren's String doesn't
  // implement `<`; List.sort needs a comparator.
  static stringLT_(a, b) {
    var ab = a.bytes
    var bb = b.bytes
    var n  = ab.count.min(bb.count)
    var i = 0
    while (i < n) {
      if (ab[i] != bb[i]) return ab[i] < bb[i]
      i = i + 1
    }
    return ab.count < bb.count
  }

  // ----- SFTP loaders --------------------------------------------------

  // Resolve the starting cwd.  Prefer the server-advertised pubdir,
  // fall back to realpath("."), then "/".  Yields once on the realpath
  // round-trip when pubdir is unset.  The `|| Fiber.yield()` pattern is
  // mandatory — SFTP.* returns null when the request was queued (the
  // common path) but returns SFTPError immediately on sync failure
  // (session dead, OOM); we must propagate the latter without yielding.
  static initialCwd() {
    var pd = SFTP.pubdir
    if (pd != null && pd.count > 0) return pd
    var r = SFTP.realpath(Fiber.current, ".") || Fiber.yield()
    if (r is String && r.count > 0) return r
    return "/"
  }

  // Open `path`, drain readdir until EOF (null reply), close.  Returns
  // a List<Row> or an SFTPError.
  static loadDir(path) {
    var h = SFTP.opendir(Fiber.current, path) || Fiber.yield()
    if (h is SFTPError) return h

    var entries = []
    var more = true
    while (more) {
      var batch = SFTP.readdir(Fiber.current, h) || Fiber.yield()
      if (batch == null) {
        more = false
      } else if (batch is SFTPError) {
        SFTP.close(Fiber.current, h) || Fiber.yield()
        return batch
      } else {
        for (e in batch) {
          // Server-side "." / ".." are filtered out — we synthesize
          // ".." ourselves.
          if (e.name != "." && e.name != "..") entries.add(e)
        }
      }
    }

    SFTP.close(Fiber.current, h) || Fiber.yield()

    entries.sort {|a, b|
      if (a.isDir != b.isDir) return a.isDir
      return stringLT_(a.name, b.name)
    }

    var rows = []
    if (path != "/") {
      rows.add(Row.new("..", true, true, 0, 0, null, false,
                       formatRow("..", true, 0, 0, null, false)))
    }
    for (e in entries) {
      var line  = formatRow(e.name, e.isDir, e.size, e.mtime,
                            e.longname, e.hasLongDesc)
      // lname is the short description ONLY for files; for dirs it
      // was already consumed as the row label.
      var sdesc = e.isDir ? null : e.longname
      rows.add(Row.new(e.name, e.isDir, false, e.size, e.mtime,
                       sdesc, e.hasLongDesc, line))
    }
    return rows
  }

  // ----- Entry point ---------------------------------------------------

  static run() {
    if (!SFTP.available) return
    var savedScreen  = Screen.save()
    var wasSuspended = CTerm.suspended
    CTerm.suspended  = true

    runBody_()

    CTerm.suspended = wasSuspended
    Screen.restore(savedScreen)
  }

  static runBody_() {
    var app  = App.new()
    var size = Screen.size

    // Pane fills the working area minus a top margin; SyncTERM's own
    // bottom status line lives at row size[1] and stays clear so it
    // doesn't get clobbered.
    var pane = Pane.new()
    pane.bounds      = Rect.new(2, 2, size[0] - 2, size[1] - 2)
    pane.focused     = true
    pane.titleAsBar  = false       // title in the top border, no inset row
    pane.onClose     = Fn.new { app.quit() }
    pane.helpText =
        "SFTP browser\n\n" +
        "Up / Down       move the highlight\n" +
        "Home / End      jump to ends\n" +
        "PageUp/PageDown scroll a page\n" +
        "Enter           descend into a directory; .. goes up\n" +
        "F2              show long description (rows marked '+')\n" +
        "Esc / [X]       leave the browser\n" +
        "F1 / [?]        this help\n\n" +
        "Files cannot be transferred from this version yet — that\n" +
        "comes with the Wren queue replacement.  For now this is a\n" +
        "navigation-only browser."
    app.root.add(pane)

    // List takes all of the inner area except the last row, which is
    // a description bar fed asynchronously from the descs@syncterm.net
    // extension.  Putting the bar inside the pane (rather than below
    // it) keeps SyncTERM's own bottom status line intact.
    var ib   = pane.innerBounds
    var list = SftpListView.new()
    list.bounds = Rect.new(ib.x, ib.y, ib.w, ib.h - 1)
    pane.add(list)

    var descBar = StatusBar.new()
    descBar.bounds = Rect.new(ib.x, ib.y + ib.h - 1, ib.w, 1)
    pane.add(descBar)

    // cwd lives in a single-element list so onSelect's closure can
    // mutate it.  Wren upvalue assignment through nested Fns is
    // brittle; the indirection is the idiomatic fix.
    var cwd = [initialCwd()]

    // Show the row's cached short description (the lname extension
    // value, populated only on file rows).  No SFTP round-trip — the
    // server hands the short desc back in the readdir attrs.
    var setDesc = Fn.new {|item|
      if (item == null || item.shortDesc == null ||
          item.shortDesc.count == 0) {
        descBar.text = ""
        return
      }
      descBar.segments = [[item.shortDesc, "center"]]
    }

    list.onSelectionChange = setDesc

    var refresh = Fn.new {
      app.popStatus("Loading %(cwd[0]) ...")
      var r = loadDir(cwd[0])
      app.popStatus(null)
      pane.title = "SFTP: %(cwd[0])"
      if (r is SFTPError) {
        Alert.show(app, "Cannot list %(cwd[0]):\n" + r.toString)
        if (list.items.count == 0) list.items = []
        setDesc.call(null)
        return
      }
      list.items = r
      // ListView.items= bypasses the selected setter, so
      // onSelectionChange doesn't fire for a fresh listing — drive the
      // description bar manually for the just-selected first row.
      setDesc.call(list.selectedItem)
    }

    list.onSelect = Fn.new {|i, item|
      if (!item.isDir) return    // files: no-op for now
      if (item.isParent) {
        cwd[0] = parentPath(cwd[0])
      } else {
        cwd[0] = joinPath(cwd[0], item.name)
      }
      refresh.call()
    }

    // F2 — fetch and display the selected file's long description
    // via the descs@syncterm.net request.  Server marked the row's
    // hasLongDesc when its readdir reply included a presence-only
    // descs ext on the fattr; if the marker is absent we don't
    // bother with the round-trip.
    app.bind(Key.f2, Fn.new {|k|
      var item = list.selectedItem
      if (item == null || item.isDir || item.isParent) return
      if (!item.hasLongDesc) {
        Alert.show(app, "No long description.")
        return
      }
      var path = joinPath(cwd[0], item.name)
      app.popStatus("Fetching description ...")
      var d = SFTP.descs(Fiber.current, path) || Fiber.yield()
      app.popStatus(null)
      if (d is SFTPError) {
        Alert.show(app, "Cannot fetch description:\n" + d.toString)
        return
      }
      if (d == null || d.count == 0) {
        Alert.show(app, "(empty long description)")
        return
      }
      // File descriptions are almost always preformatted (column-
      // aligned text, ASCII art, etc.).  Block-centre instead of
      // centring each line individually.
      Alert.showPreformatted(app, d)
    })

    app.bind(Key.escape, Fn.new {|k| app.quit() })

    refresh.call()
    app.run()
  }
}

Hook.onKey(0x1F00) { |k|   // Alt-S
  if (!SFTP.available) return false
  Fiber.new { SftpBrowser.run() }.call()
  return true
}
