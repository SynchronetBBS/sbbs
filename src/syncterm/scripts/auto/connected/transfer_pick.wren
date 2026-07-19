// transfer_pick.wren -- upload and download selection flows. Each App
// pops a ListView for the protocol, optionally a file picker or
// Prompt for a filename, then dispatches to Transfer.upload /
// Transfer.uploadBatch / Transfer.download.  The dispatched call
// blocks (worker thread + main-thread TransferApp loop) until the
// transfer dismisses, so the picker callsite is naturally serialized
// with the transfer itself.
//
// UploadApp.run(autoZ, lastCh):
//   autoZ == true skips the protocol picker (the auto-ZRQINIT
//   trigger in doterm() already knows it's ZMODEM).  lastCh is the
//   trailing wire byte fed to the xmodem recv state when the
//   protocol falls back; ignored for ZMODEM / ASCII / Raw.
//
// DownloadApp.run():
//   Always pops the protocol picker.  CET Telesoftware is offered
//   only when CTerm.emulation == Emulation.prestel (matches
//   begin_download's gate).  For plain XMODEM the user is prompted
//   for a filename first (the wire stream has no header).

import "syncterm" for BBS, CTerm, Emulation, FilePickerOptions, Host,
                       Input, Screen, Transfer
import "ui_app"     for App
import "ui_pane"    for Pane
import "ui_picker"  for ListPicker
import "ui_popup"   for Prompt

class UploadApp {
  // Index → kind name in the order shown to the user.
  static UPLOAD_KINDS_ {
    return [
      "zmodem",       // ZMODEM
      "zmodem",       // ZMODEM Batch  (Transfer.uploadBatch)
      "ymodem",       // YMODEM
      "ymodem",       // YMODEM Batch  (Transfer.uploadBatch)
      "xmodem-1k",    // XMODEM-1K
      "xmodem-128",   // XMODEM-128
      "ascii",        // ASCII
      "raw"           // Raw
    ]
  }

  static UPLOAD_LABELS_ {
    return [
      "ZMODEM", "ZMODEM Batch", "YMODEM", "YMODEM Batch",
      "XMODEM-1K", "XMODEM-128", "ASCII", "Raw"
    ]
  }

  static run(autoZ, lastCh) {
    Fiber.new {
      Screen.modalRun(Fn.new {
        if (Host.safeMode) {
          return
        }
        var proto = autoZ ? 0 : pickProtocol_(UploadApp.UPLOAD_LABELS_,
            "Upload Protocol", uploadHelp_())
        if (proto < 0) return

        var batch = (proto == 1) || (proto == 3)
        var kind  = UploadApp.UPLOAD_KINDS_[proto]

        if (batch) {
          var files = Host.pickFiles(BBS.ulDir, null, 0)
          if (files == null || files.count == 0) return
          var paths = files.map {|f| f.toString }.toList
          Transfer.uploadBatch(kind, paths, lastCh)
        } else {
          // Let the user type a path outside the configured upload
          // directory as well as choose a listed file.
          var file = Host.pickFile(BBS.ulDir, null,
              FilePickerOptions.allowEntry)
          if (file == null) return
          Transfer.upload(kind, file.toString, lastCh)
        }
      })
      Input.setupMouseEvents()
    }.call()
  }

  static uploadHelp_() {
    return "# Upload Protocol\n" +
           "\n" +
           "- **ZMODEM**: streaming protocol, auto-detected by most BBSes.\n" +
           "- **YMODEM**: 1K blocks with filename header.\n" +
           "- **XMODEM-1K / -128**: simple block protocols; no filename\n" +
           "  in the wire stream so the receiver picks the name.\n" +
           "- **ASCII**: plain text, line-by-line; no error checking.\n" +
           "- **Raw**: byte-for-byte; no error checking, no flow control."
  }

  static pickProtocol_(items, title, helpText) {
    return ListPicker.pick(title, helpText, items)
  }
}

class DownloadApp {
  // Indices into the displayed list -- CET is conditionally appended.
  static DOWNLOAD_BASE_KINDS_ {
    return [
      "zmodem",         // ZMODEM
      "ymodem-g",       // YMODEM-g
      "ymodem",         // YMODEM
      "xmodem-crc",     // XMODEM-CRC      (needs filename)
      "xmodem-chksum"   // XMODEM-CHKSUM   (needs filename)
    ]
  }

  static DOWNLOAD_BASE_LABELS_ {
    return [
      "ZMODEM", "YMODEM-g", "YMODEM",
      "XMODEM-CRC", "XMODEM-CHKSUM"
    ]
  }

  static run() {
    Fiber.new {
      Screen.modalRun(Fn.new {
        if (Host.safeMode) {
          return
        }

        var labels = DownloadApp.DOWNLOAD_BASE_LABELS_
        var kinds  = DownloadApp.DOWNLOAD_BASE_KINDS_
        if (CTerm.emulation == Emulation.prestel) {
          labels = labels + ["CET Telesoftware"]
          kinds  = kinds  + ["cet"]
        }
        var proto = UploadApp.pickProtocol_(labels, "Download Protocol",
            downloadHelp_())
        if (proto < 0) return

        var kind = kinds[proto]
        if (kind == "xmodem-crc" || kind == "xmodem-chksum") {
          var name = askFilename_()
          if (name == null || name.isEmpty) return
          Transfer.download(kind, name)
        } else {
          Transfer.download(kind, null)
        }
      })
      Input.setupMouseEvents()
    }.call()
  }

  // Wraps Prompt.show in its own App so the dismissal returns
  // synchronously to the picker flow.  Returns the entered filename
  // (or null on cancel / empty input).
  static askFilename_() {
    var app  = App.new()
    var pane = Pane.new()
    pane.title = "XMODEM Download"
    pane.helpText =
      "# XMODEM filename\n\n" +
      "Plain XMODEM has no filename in the wire stream, so SyncTERM\n" +
      "saves the captured bytes to whatever name you type here under\n" +
      "the BBS download directory."
    app.root.add(pane)
    pane.fitContent()
    pane.centerOnScreen()
    return Prompt.runStandalone(app, "Filename", "")
  }

  static downloadHelp_() {
    return "# Download Protocol\n" +
           "\n" +
           "- **ZMODEM**: streaming, with crash recovery.\n" +
           "- **YMODEM-g**: streaming variant; faster but no recovery.\n" +
           "- **YMODEM**: 1K blocks with filename header.\n" +
           "- **XMODEM-CRC / -CHKSUM**: simple block protocols; you\n" +
           "  must pre-name the file (no filename in the stream).\n" +
           "- **CET Telesoftware**: Prestel-only file capture; the\n" +
           "  captured frame is replayed to the terminal on completion."
  }
}
