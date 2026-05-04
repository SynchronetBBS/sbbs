// SFTP UI — single App with two modes (browser, queue) and a
// modal upload picker.  Replaces the previous SftpBrowser /
// QueueScreen / SftpQueueDegraded triplet (each its own App)
// with one stateful App that swaps panes on Alt-S / Alt-Q.
//
// Modes are lazy-cached: a mode's pane + state are built the
// first time setMode switches to it and stay alive when switching
// away, so switch-back is instant.  The queue mode's state is
// driven by a tickMs poll on SftpQueue.gen — inactive while
// browser mode is up (no rebuilds, no repaints), refreshed on
// switch-to-queue and on every gen change while queue mode is
// active.
//
// Shell-close UX: shell-close fires onShellClose; the handler
// ensures SftpApp is up in queue mode and sets `shellClosed = true`.
// That flag changes the Esc / [X] semantic — instead of just closing
// the App (live-session behavior, transfers continue in background),
// the close path also calls SftpQueue.suspend(), which exits worker
// fibers without flipping ACTIVE → terminal.  Jobs persist in their
// ACTIVE state and resume from `done` on the next session.
// CTerm.sftpActive clears as part of suspend(), so is_connected goes
// false and the term.c disconnect path can run normally.

import "syncterm" for CTerm, Download, File, Format, Host, Key,
                       KeyEvent, Screen, SFTP, SFTPError
import "ui_app"    for App
import "ui_pane"   for Pane
import "ui_list"   for ListView
import "ui_widget" for Rect
import "ui_popup"  for Alert
import "ui_statusbar" for StatusBar
import "sftp_queue"   for SftpQueue

// ----- Helpers (formatters + path math, all static) -----------------

class SftpFmt {
  // ----- Path joins / splits (browser) ----------------------------

  static joinPath(base, name) {
    if (base.count == 0) return "/" + name
    if (base[-1] == "/") return base + name
    return base + "/" + name
  }

  // Strip the last path component.  Byte-indexed so trailing /
  // and leading / trimming is unambiguous on UTF-8 paths.
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

  // Last path component.  Byte-indexed (paths are typically ASCII).
  static basename(path) {
    if (path == null || path.count == 0) return ""
    var n = path.bytes.count
    var i = n
    while (i > 0 && path[i - 1] != "/") i = i - 1
    return path[i...n]
  }

  // ----- Display formatting (browser) -----------------------------

  // 10-char right-aligned size column.  Directories render as
  // <DIR>; files use a binary-suffix abbreviation.
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

  // YYYY-MM-DD UTC, or 10 spaces when mtime is zero / missing.
  // Howard Hinnant's days-from-civil inverse; Wren has no time API.
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

  // Display label rules — directories prefer the lname (friendly
  // name); files always show their bare filename, since their
  // lname is the short description that goes in the bottom bar
  // instead.  hasLongDesc shows a `+` marker so users know F2 has
  // something to fetch.  `chip` is the 4-char transfer / local-vs-
  // remote status block (see statusChip below) — pre-rendered so
  // we can refresh just that column when SftpQueue.gen advances.
  static formatBrowserRow(name, isDir, size, mtime, lname, hasLongDesc, chip) {
    var label = name
    if (isDir && lname != null && lname.count > 0) label = lname
    var mark = hasLongDesc ? "+" : " "
    return "%(formatSize(size, isDir)) %(formatDate(mtime)) %(chip) %(mark) %(label)"
  }

  // 4-char status chip for one row, mirroring the C-side
  // sftp_browser.c rebuild_status_chip:
  //
  //   "[↓↓]"  active download
  //   "[↑↑]"  active upload
  //   "[Q↓]"  queued download
  //   "[Q↑]"  queued upload
  //   "[er]"  failed
  //   "[cx]"  cancelled
  //   "[==]"  local copy size matches AND (hash matches when the
  //           server advertised one, else mtime matches)
  //   "[<>]"  local copy exists but differs (size, hash, or mtime)
  //   "    "  no local copy or directory / parent
  //
  // `remote` is the absolute server path (used for queue lookups);
  // `localName` is the basename inside Download (used for the
  // local-state compare).  hash is null when neither sha1s nor
  // md5s was negotiated.  Directories and ".." get a blank chip.
  static statusChip(isDir, isParent, localName, remote, size, mtime, hash) {
    if (isDir || isParent || Download == null) return "    "
    var dnSt = SftpQueue.status(SftpQueue.DOWNLOAD, remote)
    var upSt = SftpQueue.status(SftpQueue.UPLOAD, remote)
    if (dnSt == SftpQueue.ACTIVE)    return "[↓↓]"
    if (upSt == SftpQueue.ACTIVE)    return "[↑↑]"
    if (dnSt == SftpQueue.QUEUED)    return "[Q↓]"
    if (upSt == SftpQueue.QUEUED)    return "[Q↑]"
    if (dnSt == SftpQueue.FAILED    || upSt == SftpQueue.FAILED)    return "[er]"
    if (dnSt == SftpQueue.CANCELLED || upSt == SftpQueue.CANCELLED) return "[cx]"
    if (!Download.contains(localName)) return "    "
    var lf = Download.list[localName]
    if (!(lf is File)) return "    "
    if (lf.size != size) return "[<>]"
    if (hash != null) {
      // sha1s/md5s extension was negotiated — authoritative compare.
      var local = (hash.count == 20) ? lf.sha1 : (hash.count == 16 ? lf.md5 : null)
      if (local == null) return "    "
      return (local == hash) ? "[==]" : "[<>]"
    }
    // No server hash — fall back to mtime equality.
    return (lf.mtime == mtime) ? "[==]" : "[<>]"
  }

  // Byte-wise less-than for two Strings.  Wren's String doesn't
  // implement `<`; List.sort needs a comparator.
  static stringLT(a, b) {
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

  // ----- SFTP loaders (browser) -----------------------------------

  // Resolve the starting cwd.  Prefer the server-advertised pubdir,
  // fall back to realpath("."), then "/".  Yields once on the realpath
  // round-trip when pubdir is unset.
  static initialCwd() {
    var pd = SFTP.pubdir
    if (pd != null && pd.count > 0) return pd
    var r = SFTP.realpath(Fiber.current, ".") || Fiber.yield()
    if (r is String && r.count > 0) return r
    return "/"
  }

  // Open `path`, drain readdir until EOF (null reply), close.
  // Returns a List<BrowserRow> or an SFTPError.
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
          if (e.name != "." && e.name != "..") entries.add(e)
        }
      }
    }

    SFTP.close(Fiber.current, h) || Fiber.yield()

    entries.sort {|a, b|
      if (a.isDir != b.isDir) return a.isDir
      return stringLT(a.name, b.name)
    }

    var rows = []
    if (path != "/") {
      rows.add(BrowserRow.new("..", true, true, 0, 0,
                              null, null, false, null, null))
    }
    for (e in entries) {
      var sdesc  = e.isDir ? null : e.longname
      var remote = (path == "/") ? "/" + e.name : path + "/" + e.name
      rows.add(BrowserRow.new(e.name, e.isDir, false, e.size, e.mtime,
                              e.longname, sdesc, e.hasLongDesc,
                              e.hash, remote))
    }
    return rows
  }

  // ----- Display formatting (queue) -------------------------------

  static statusOrder(s) {
    if (s == SftpQueue.ACTIVE)    return 0
    if (s == SftpQueue.QUEUED)    return 1
    if (s == SftpQueue.DONE)      return 2
    if (s == SftpQueue.FAILED)    return 3
    if (s == SftpQueue.CANCELLED) return 4
    return 5
  }

  static formatPct(done, total, status) {
    if (status == SftpQueue.DONE)      return "(100\%)"
    if (status == SftpQueue.FAILED)    return "( --\%)"
    if (status == SftpQueue.CANCELLED) return "(can\%)"
    if (total <= 0) return "(  -\%)"
    var p = ((done * 100) / total).floor
    if (p > 99) p = 99
    if (p < 10) return "(  %(p)\%)"
    return "( %(p)\%)"
  }

  static padRight(s, n) {
    var r = s
    while (r.count < n) r = r + " "
    return r
  }

  static formatQueueRow(snap) {
    var arrow  = (snap.dir == SftpQueue.UPLOAD) ? "↑" : "↓"
    // Direction arrow between local and remote follows the data flow:
    // upload (local → remote), download (local ← remote).
    var flow   = (snap.dir == SftpQueue.UPLOAD) ? "→" : "←"
    var status = padRight(snap.status, 9)
    var bytes  = Format.bytes(snap.done) + "/" + Format.bytes(snap.total)
    var pct    = formatPct(snap.done, snap.total, snap.status)
    var name   = basename(snap.local)
    if (snap.status == SftpQueue.FAILED && snap.errMsg.count > 0) {
      return "%(arrow) %(status) %(pct) %(name) %(flow) %(snap.remote)  [%(snap.errMsg)]"
    }
    // bpsStr / etaStr are populated only on ACTIVE jobs that have
    // moved bytes since going active; pad to fixed width so non-ACTIVE
    // rows still align with ACTIVE ones.
    var rate = padRight(snap.bpsStr, 9)
    var eta  = padRight(snap.etaStr, 6)
    return "%(arrow) %(status) %(padRight(bytes, 14)) %(pct) %(rate) %(eta) %(name) %(flow) %(snap.remote)"
  }
}

// ----- Row + ListView helpers ---------------------------------------

class BrowserRow {
  construct new(name, isDir, isParent, size, mtime, lname, shortDesc,
                hasLongDesc, hash, remote) {
    _name        = name
    _isDir       = isDir
    _isParent    = isParent
    _size        = size
    _mtime       = mtime
    _lname       = lname        // for label rendering
    _shortDesc   = shortDesc    // for description bar
    _hasLongDesc = hasLongDesc
    _hash        = hash         // for status-chip compare; null when no sha1s/md5s
    _remote      = remote       // absolute server path; null for ".." parent
    refresh()
  }
  name        { _name        }
  isDir       { _isDir       }
  isParent    { _isParent    }
  size        { _size        }
  mtime       { _mtime       }
  shortDesc   { _shortDesc   }
  hasLongDesc { _hasLongDesc }
  hash        { _hash        }
  remote      { _remote      }
  line        { _line        }

  // Recompute the chip and rebuild the display line.  Called from
  // construct + on every SftpQueue.gen change while the row is in
  // a visible browser-mode list.
  refresh() {
    var chip = SftpFmt.statusChip(_isDir, _isParent, _name, _remote,
                                  _size, _mtime, _hash)
    _line = SftpFmt.formatBrowserRow(_name, _isDir, _size, _mtime,
                                     _lname, _hasLongDesc, chip)
  }
}

// ListView whose selected= setter fires onSelectionChange so the
// description bar updates when the cursor moves.
class BrowserListView is ListView {
  construct new() {
    super()
    _onSelectionChange = null
  }
  onSelectionChange=(fn) { _onSelectionChange = fn }
  selected=(i) {
    super.selected = i
    if (_onSelectionChange != null) _onSelectionChange.call(selectedItem)
  }
  formatItem(item, width) { item.line }
}

class QueueRow {
  construct new(snap, line) {
    _snap = snap
    _line = line
  }
  snap { _snap }
  line { _line }
}

class QueueListView is ListView {
  construct new() { super() }
  formatItem(item, width) { item.line }
}

// ----- The App ------------------------------------------------------

class SftpApp is App {
  // Currently-running SftpApp, or null.  sftp_queue_init.wren reads
  // this from onShellClose to switch the running App into queue
  // mode and flag it shellClosed (changing the Esc semantic to
  // "stop transfers + persist + disconnect").  Set for the
  // duration of run().
  static current { __current }

  construct new(mode) {
    super()
    _mode         = null
    _shellClosed  = false  // raised by onShellClose; flips Esc semantic
    tickMs        = 100    // drives queue-mode polling via onTick_

    // Browser-mode state — null until ensureBrowser_() builds it.
    _bPane    = null
    _bList    = null
    _bDescBar = null
    _bCwd     = null
    _bLastGen = -1   // last SftpQueue.gen we refreshed chips against

    // Queue-mode state — null until ensureQueue_() builds it.
    _qPane     = null
    _qList     = null
    _qLastGen  = -1

    // Mode-switch hotkeys — bound once, valid in either mode.
    bind(Key.altS, Fn.new {|k| setMode("browser") })
    bind(Key.altQ, Fn.new {|k| setMode("queue") })
    bind(Key.escape, Fn.new {|k| dismiss_() })

    // F2 / F4 — browser-only operations.  No-op silently in queue
    // mode (rare keystroke; quieter than throwing).
    bind(Key.f2, Fn.new {|k|
      if (_mode == "browser") fetchDesc_()
    })
    bind(Key.f4, Fn.new {|k|
      if (_mode == "browser") pickUpload_()
    })

    // Del - queue-only operation.  No-op silently in browser mode.
    bind(Key.delChar, Fn.new {|k|
      if (_mode == "queue") cancelSelected_()
    })

    setMode(mode)
  }

  mode { _mode }

  // Set by sftp_queue_init.wren's onShellClose handler.  Flips Esc /
  // [X] from "just close the App" to "suspend workers + close" so the
  // disconnect path can run; jobs left ACTIVE persist for resume on
  // the next session.  When false (live shell), close paths are
  // benign — transfers continue in the background.
  shellClosed=(b) { _shellClosed = b }

  // ----- Mode switching -------------------------------------------

  setMode(m) {
    if (m == _mode) return
    if (m == "browser" && !SFTP.available) return

    // Detach the current mode's pane (state stays alive).
    if (_mode == "browser" && _bPane != null) root.remove(_bPane)
    if (_mode == "queue"   && _qPane != null) root.remove(_qPane)

    _mode = m
    if (m == "browser") {
      ensureBrowser_()
      root.add(_bPane)
      _bPane.focused = true
    } else if (m == "queue") {
      ensureQueue_()
      root.add(_qPane)
      _qPane.focused = true
      // Force a rebuild on switch-in so the visible state reflects
      // any progress that ticked while browser mode was up.
      rebuildQueue_()
      _qLastGen = SftpQueue.gen
    }
  }

  // ----- Close handling -------------------------------------------

  // Esc / [X] / Pane.onClose route here.  In a live session this is
  // just `quit()` - transfers keep running in their worker fibers.
  // After shell-close, the user wants out: suspend the workers
  // (they exit at next chunk-boundary check, leaving ACTIVE jobs
  // ACTIVE), then quit.  CTerm.sftpActive clears as part of
  // suspend(), so is_connected goes false and the disconnect path
  // proceeds.  onDisconnect's stop() persists the still-ACTIVE
  // jobs; load_ on the next session converts them back to QUEUED
  // and resumes from `done`.
  dismiss_() {
    if (_shellClosed) SftpQueue.suspend()
    quit()
  }

  // ----- Tick: queue-mode poll ------------------------------------

  // Fires every tickMs (100 ms) from the App's own pump.  In queue
  // mode, rebuilds when SftpQueue.gen advances.  In browser mode,
  // refreshes the per-row status chips so users can watch transfer
  // state change in place (queued -> active -> done) without
  // navigating to the queue.
  onTick_() {
    if (_mode == "queue") {
      if (_qPane == null) return
      var g = SftpQueue.gen
      if (g != _qLastGen) {
        _qLastGen = g
        rebuildQueue_()
      }
      // Auto-quit when the shell has closed and there's nothing more
      // to wait on: either the queue has fully drained (all jobs hit
      // a terminal state) or the workers have suspended (server tore
      // down the SFTP channel mid-transfer; jobs preserved ACTIVE for
      // resume).  Either way the App has nothing left to show; falling
      // through clears CTerm.sftpActive so the disconnect path runs.
      if (_shellClosed && (SftpQueue.suspended || !SftpQueue.hasWork)) {
        quit()
      }
    } else if (_mode == "browser") {
      if (_bList == null) return
      var g = SftpQueue.gen
      if (g != _bLastGen) {
        _bLastGen = g
        refreshChips_()
      }
    }
  }

  // Walk the visible row list and re-render each row's chip from
  // current queue + local-file state.  Marks the list dirty + posts
  // a wake so the change paints on the next drain.
  refreshChips_() {
    for (r in _bList.items) r.refresh()
    _bList.markDirty()
    post()
  }

  // ----- Browser-mode setup + operations --------------------------

  ensureBrowser_() {
    if (_bPane != null) return
    var size = Screen.size

    var pane = Pane.new()
    pane.bounds      = Rect.new(2, 2, size[0] - 2, size[1] - 2)
    pane.titleAsBar  = false
    pane.onClose     = Fn.new { dismiss_() }
    pane.helpText =
        "# SFTP browser\n\n" +
        "## Keys\n\n" +
        "Up / Down\n" +
        ":  move the highlight\n" +
        "Home / End\n" +
        ":  jump to ends\n" +
        "PageUp / PageDown\n" +
        ":  scroll a page\n" +
        "Enter\n" +
        ":  on a directory: descend (`..` goes up); on a file: queue for download\n" +
        "F2\n" +
        ":  show long description (rows marked `+`)\n" +
        "F4\n" +
        ":  pick a file from `UploadPath` and queue it as an upload to the current directory\n" +
        "Alt-Q\n" +
        ":  switch to queue mode\n" +
        "Esc / [X]\n" +
        ":  leave the SFTP UI\n" +
        "F1 / [?]\n" +
        ":  this help\n" +
        "\n" +
        "## Status block\n" +
        "\n" +
        "The 4-char block before each filename shows transfer + local-vs-remote state:\n" +
        "\n" +
        "[↓↓]\n" +
        ":  downloading\n" +
        "[↑↑]\n" +
        ":  uploading\n" +
        "[Q↓]\n" +
        ":  queued down\n" +
        "[Q↑]\n" +
        ":  queued up\n" +
        "[er]\n" +
        ":  failed\n" +
        "[cx]\n" +
        ":  cancelled\n" +
        "[==]\n" +
        ":  local matches\n" +
        "[<>]\n" +
        ":  local differs\n" +
        "(blank)\n" +
        ":  no local copy in `DownloadPath`"
    _bPane = pane

    // Description bar: only worth its own row when the lname
    // extension is negotiated (the bar's content is the per-file
    // shortDesc that lname carries — without it, the bar is always
    // blank).  When absent, the list takes the full inner area.
    var ib       = pane.innerBounds
    var hasLname = SFTP.lname
    var listH    = hasLname ? ib.h - 1 : ib.h
    var list = BrowserListView.new()
    list.bounds = Rect.new(ib.x, ib.y, ib.w, listH)
    pane.add(list)
    _bList = list

    if (hasLname) {
      var descBar = StatusBar.new()
      descBar.bounds = Rect.new(ib.x, ib.y + ib.h - 1, ib.w, 1)
      pane.add(descBar)
      _bDescBar = descBar
    } else {
      _bDescBar = null
    }

    list.onSelectionChange = Fn.new {|item| updateDescBar_(item) }

    list.onSelect = Fn.new {|i, item|
      if (item.isDir) {
        if (item.isParent) {
          _bCwd = SftpFmt.parentPath(_bCwd)
        } else {
          _bCwd = SftpFmt.joinPath(_bCwd, item.name)
        }
        refreshBrowser_()
        return
      }
      // File: enqueue for download.  Idempotent on (DOWNLOAD, remote).
      if (Download == null) {
        Alert.show(this,
            "DownloadPath is not configured, is set to $HOME, or " +
            "doesn't exist on disk.  Set DownloadPath in your BBS list " +
            "(and create the directory if necessary) before downloading.")
        return
      }
      var remote   = SftpFmt.joinPath(_bCwd, item.name)
      var existing = SftpQueue.status(SftpQueue.DOWNLOAD, remote)
      if (existing == SftpQueue.QUEUED || existing == SftpQueue.ACTIVE) {
        showDescBar_("Already queued: %(item.name)")
        return
      }
      SftpQueue.enqueue(SftpQueue.DOWNLOAD, item.name, remote, item.size)
      showDescBar_("Queued: %(item.name)")
    }

    _bCwd = runChild(Fn.new { SftpFmt.initialCwd() })
    refreshBrowser_()
  }

  refreshBrowser_() {
    popStatus("Loading %(_bCwd) ...")
    var cwd = _bCwd
    var r = runChild(Fn.new { SftpFmt.loadDir(cwd) })
    popStatus(null)
    _bPane.title = "SFTP: %(_bCwd)"
    if (r is SFTPError) {
      Alert.show(this, "Cannot list %(_bCwd):\n" + r.toString)
      if (_bList.items.count == 0) _bList.items = []
      updateDescBar_(null)
      return
    }
    _bList.items = r
    _bLastGen = SftpQueue.gen   // baseline for chip-refresh polling
    // ListView.items= bypasses the selected= setter, so onSelectionChange
    // doesn't fire for the initial selection — drive the bar manually.
    updateDescBar_(_bList.selectedItem)
  }

  updateDescBar_(item) {
    if (_bDescBar == null) return
    if (item == null || item.shortDesc == null ||
        item.shortDesc.count == 0) {
      _bDescBar.text = ""
      return
    }
    _bDescBar.segments = [[item.shortDesc, "center"]]
  }

  // Show a transient status string in the description bar — used
  // for "Queued: X" / "Already queued: X" feedback.  No-op when
  // the bar isn't present (lname not negotiated); the user can
  // verify in queue mode instead.
  showDescBar_(text) {
    if (_bDescBar == null) return
    _bDescBar.segments = [[text, "center"]]
  }

  fetchDesc_() {
    var item = _bList.selectedItem
    if (item == null || item.isDir || item.isParent) return
    if (!item.hasLongDesc) {
      Alert.show(this, "No long description.")
      return
    }
    var path = SftpFmt.joinPath(_bCwd, item.name)
    popStatus("Fetching description ...")
    var d = runChild(Fn.new { SFTP.descs(Fiber.current, path) || Fiber.yield() })
    popStatus(null)
    if (d is SFTPError) {
      Alert.show(this, "Cannot fetch description:\n" + d.toString)
      return
    }
    if (d == null || d.count == 0) {
      Alert.show(this, "(empty long description)")
      return
    }
    Alert.showPreformatted(this, d)
  }

  pickUpload_() {
    releaseFocus()
    var picked = Host.pickFiles(Host.uploadPath, "*", 0)
    restoreFocus()
    if (picked == null || picked.count == 0) return
    var queued  = 0
    var skipped = 0
    for (f in picked) {
      var local  = f.toString
      var name   = SftpFmt.basename(local)
      var remote = SftpFmt.joinPath(_bCwd, name)
      var existing = SftpQueue.status(SftpQueue.UPLOAD, remote)
      if (existing == SftpQueue.QUEUED || existing == SftpQueue.ACTIVE) {
        skipped = skipped + 1
        continue
      }
      f.open()
      var sz = f.size
      f.close()
      SftpQueue.enqueue(SftpQueue.UPLOAD, local, remote, sz, f.token)
      queued = queued + 1
    }
    if (queued == 0 && skipped == 0) return
    if (skipped == 0) {
      showDescBar_("Queued upload: %(queued) file(s)")
    } else {
      showDescBar_("Queued: %(queued); already queued: %(skipped)")
    }
  }

  // ----- Queue-mode setup + operations ----------------------------

  ensureQueue_() {
    if (_qPane != null) return
    var size = Screen.size

    var pane = Pane.new()
    pane.bounds     = Rect.new(2, 2, size[0] - 2, size[1] - 2)
    pane.titleAsBar = false
    pane.title      = "SFTP Queue"
    pane.onClose    = Fn.new { dismiss_() }
    pane.helpText   =
        "# SFTP queue\n\n" +
        "## Keys\n\n" +
        "Up / Down\n" +
        ":  move the highlight\n" +
        "Home / End\n" +
        ":  jump to ends\n" +
        "PageUp / PageDown\n" +
        ":  scroll a page\n" +
        "Del\n" +
        ":  cancel the highlighted job\n" +
        "Alt-S\n" +
        ":  switch to browser mode (if SFTP is available)\n" +
        "Esc / [X]\n" +
        ":  leave the SFTP UI (also stops transfers and ends the session if the shell has closed)\n" +
        "F1 / [?]\n" +
        ":  this help\n" +
        "\n" +
        "Active and queued jobs are at the top; finished, failed, and cancelled jobs are listed below.  " +
        "Cancelling an active job takes effect at the next chunk boundary."
    _qPane = pane

    var ib   = pane.innerBounds
    var list = QueueListView.new()
    list.bounds = Rect.new(ib.x, ib.y, ib.w, ib.h)
    pane.add(list)
    _qList = list
  }

  rebuildQueue_() {
    var snaps = SftpQueue.snapshot
    var rows = []
    // Stable sort by status bucket; insertion order preserved within
    // each bucket.
    for (bucket in 0..4) {
      for (s in snaps) {
        if (SftpFmt.statusOrder(s.status) == bucket) {
          rows.add(QueueRow.new(s, SftpFmt.formatQueueRow(s)))
        }
      }
    }
    // Preserve selection across rebuild — match by (dir, remote).
    var prevDir    = null
    var prevRemote = null
    var sel = _qList.selectedItem
    if (sel != null) {
      prevDir    = sel.snap.dir
      prevRemote = sel.snap.remote
    }
    _qList.items = rows
    if (prevRemote != null) {
      for (i in 0...rows.count) {
        var r = rows[i]
        if (r.snap.dir == prevDir && r.snap.remote == prevRemote) {
          _qList.selected = i
          break
        }
      }
    }
    // Mode switching can leave us drawing a stale frame; an explicit
    // post wakes the run-fiber so the rebuilt list paints in the
    // next iteration.
    post()
  }

  cancelSelected_() {
    var item = _qList.selectedItem
    if (item == null) return
    SftpQueue.cancel(item.snap.dir, item.snap.remote)
  }

  // ----- Lifecycle -----------------------------------------------

  // Wraps App.run with screen + CTerm-suspend save/restore (via
  // Screen.modalRun) and the static-current registration.
  // __current is set for the lifetime of run() so onShellClose can
  // find us.
  run() {
    var savedCurrent = __current
    __current = this
    Screen.modalRun(Fn.new { super.run() })
    __current = savedCurrent
    // Screen.restore re-paints the saved buffer over the status row
    // too — force a fresh status-bar repaint so live state (transfer
    // arrows, log indicators, speed) replaces the stale snapshot.
    CTerm.refreshStatus()
  }
}
